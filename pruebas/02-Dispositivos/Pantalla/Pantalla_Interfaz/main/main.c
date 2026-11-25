#include "display.h"
#include "interfaz.h"
#include "esp_log.h"
#include "estado_sistema.h"
#include <stdbool.h>
#include "protocolo_estado.h"
#include "receptor_estado.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espnow_receptor.h"
#include <string.h>
#include "esp_timer.h"
#include "driver/gpio.h"

#define USAR_ESPNOW 1
#define EJECUTAR_PRUEBAS_TEMPORALES 0
#define PIN_BOTON_SELECCION 23
#define BOTON_DEBOUNCE_US 30000LL
#define BOTON_LARGA_US 1000000LL

typedef enum {
    FASE_CALIBRACION,
    FASE_SELECCION_CAMION,
    FASE_OPERATIVA
} fase_local_t;

static fase_local_t fase = FASE_CALIBRACION;
static modelo_camion_t camion_seleccionado = CAMION_1;
static int64_t inicio_calibracion_us;
static int64_t cero_visible_desde_us;
static uint8_t segundo_visible = 15;
static bool boton_candidato;
static bool boton_estable;
static bool boton_habilitado;
static bool boton_pulsado;
static int64_t boton_cambio_us;
static int64_t boton_presionado_us;

static void preparar_boton_seleccion(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << PIN_BOTON_SELECCION,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

// Sondeo no bloqueante. Exige liberacion estable al entrar a seleccion.
static void actualizar_boton_seleccion(void)
{
    int64_t ahora = esp_timer_get_time();
    bool presionado = gpio_get_level(PIN_BOTON_SELECCION) == 0;
    if (presionado != boton_candidato) {
        boton_candidato = presionado;
        boton_cambio_us = ahora;
    }
    if (ahora - boton_cambio_us < BOTON_DEBOUNCE_US) return;
    if (!boton_habilitado) {
        if (!presionado) {
            boton_estable = false;
            boton_habilitado = true;
        }
        return;
    }
    if (presionado != boton_estable) {
        boton_estable = presionado;
        if (presionado) {
            boton_pulsado = true;
            boton_presionado_us = boton_cambio_us;
        } else if (boton_pulsado) {
            boton_pulsado = false;
            // Duracion hasta el comienzo de la liberacion ya confirmada.
            if (boton_cambio_us - boton_presionado_us >= BOTON_LARGA_US) {
                fase = FASE_OPERATIVA;
            } else {
                camion_seleccionado = camion_seleccionado == CAMION_1 ? CAMION_2 : CAMION_1;
                interfaz_mostrar_seleccion(camion_seleccionado);
            }
        }
    }
    if (boton_estable && boton_pulsado && ahora - boton_presionado_us >= BOTON_LARGA_US) {
        boton_pulsado = false;
        fase = FASE_OPERATIVA;
    }
}

// Un solo consumidor: tarea RX, o timer de simulacion cuando no se usa radio.
// Llamar con bloqueo LVGL. No espera, ni consulta/modifica receptor_estado.
static bool actualizar_fase_local(void)
{
    bool seleccion_previa = fase == FASE_SELECCION_CAMION;
    if (seleccion_previa) {
        actualizar_boton_seleccion();
        if (fase == FASE_OPERATIVA) {
            ESP_LOGI("arranque", "CAMION %u confirmado", (unsigned)camion_seleccionado + 1);
            interfaz_confirmar_seleccion(camion_seleccionado);
        }
    }
    if (fase == FASE_CALIBRACION) {
        int64_t ahora = esp_timer_get_time();
        int64_t transcurrido = ahora - inicio_calibracion_us;
        uint8_t restante = transcurrido >= 15000000LL ? 0 :
                          (uint8_t)(15 - transcurrido / 1000000LL);
        if (restante != segundo_visible) {
            interfaz_mostrar_calibracion(restante);
            segundo_visible = restante;
            if (restante == 0) cero_visible_desde_us = ahora;
        }
        if (restante == 0 && ahora - cero_visible_desde_us >= 200000LL) {
            fase = FASE_SELECCION_CAMION;
            boton_candidato = gpio_get_level(PIN_BOTON_SELECCION) == 0;
            boton_cambio_us = ahora;
            boton_habilitado = false;
            boton_pulsado = false;
            interfaz_mostrar_seleccion(camion_seleccionado);
        }
    }
    return seleccion_previa && fase == FASE_OPERATIVA;
}

#if EJECUTAR_PRUEBAS_TEMPORALES

static void registrar_prueba_receptor(const char *nombre, bool ok, unsigned *aprobados)
{
    if (ok) {
        (*aprobados)++;
        ESP_LOGI("prueba_receptor", "PASS: %s", nombre);
    } else {
        ESP_LOGE("prueba_receptor", "FAIL: %s", nombre);
    }
}

static void probar_receptor(void)
{
    unsigned aprobados = 0;
    estado_sistema_t estado;
    paquete_espnow_t paquete = {
        PROTOCOLO_ESTADO_VERSION, PROTOCOLO_MODO_EN_MOVIMIENTO,
        PROTOCOLO_RIESGO_SEGURO, 78, 42
    };
    receptor_estado_init();
    bool ok = receptor_estado_procesar(&paquete);
    ok = receptor_estado_obtener(&estado) && ok && estado.modo == MODO_EN_MOVIMIENTO &&
         estado.riesgo == RIESGO_SEGURO && estado.nivel_agua_pct == 78;
    registrar_prueba_receptor("primer paquete", ok, &aprobados);

    paquete.secuencia = 43;
    paquete.riesgo = PROTOCOLO_RIESGO_PRECAUCION;
    ok = receptor_estado_procesar(&paquete);
    ok = receptor_estado_obtener(&estado) && ok && estado.riesgo == RIESGO_PRECAUCION;
    registrar_prueba_receptor("siguiente paquete", ok, &aprobados);

    paquete.riesgo = PROTOCOLO_RIESGO_PELIGRO;
    ok = !receptor_estado_procesar(&paquete);
    ok = receptor_estado_obtener(&estado) && ok && estado.riesgo == RIESGO_PRECAUCION;
    registrar_prueba_receptor("duplicado rechazado sin cambiar estado", ok, &aprobados);
    paquete.secuencia = 42;
    ok = !receptor_estado_procesar(&paquete);
    ok = receptor_estado_obtener(&estado) && ok && estado.riesgo == RIESGO_PRECAUCION;
    registrar_prueba_receptor("anterior rechazado", ok, &aprobados);

    paquete.secuencia = (uint16_t)(43 + 0x8000);
    registrar_prueba_receptor("delta 0x8000 rechazado",
                             !receptor_estado_procesar(&paquete), &aprobados);

    receptor_estado_init();
    paquete.secuencia = 65535;
    ok = receptor_estado_procesar(&paquete);
    paquete.secuencia = 0;
    paquete.riesgo = PROTOCOLO_RIESGO_SEGURO;
    bool aceptado = receptor_estado_procesar(&paquete);
    ok = ok && aceptado;
    ok = receptor_estado_obtener(&estado) && ok && estado.riesgo == RIESGO_SEGURO;
    registrar_prueba_receptor("salto 65535 -> 0", ok, &aprobados);

    // Espera temporal solo durante las pruebas, antes de inicializar el display.
    vTaskDelay(pdMS_TO_TICKS(800));
    bool duplicado_rechazado = !receptor_estado_procesar(&paquete);
    vTaskDelay(pdMS_TO_TICKS(800));
    receptor_estado_actualizar_timeout();
    ok = receptor_estado_obtener(&estado) && estado.modo == MODO_SIN_DATOS;
    registrar_prueba_receptor("timeout sin renovacion por duplicado",
                             ok && duplicado_rechazado, &aprobados);

    paquete.secuencia = 65535;
    ok = receptor_estado_procesar(&paquete);
    ok = receptor_estado_obtener(&estado) && ok && estado.modo == MODO_EN_MOVIMIENTO;
    registrar_prueba_receptor("primer paquete tras timeout", ok, &aprobados);
    ESP_LOGI("prueba_receptor", "Resultado: %u/8 pruebas PASS", aprobados);
    receptor_estado_init();
}

// Prueba temporal: se ejecuta una sola vez, sin radio ni objetos LVGL.
static void probar_protocolo(void)
{
    static const struct {
        const char *nombre;
        uint8_t bytes[PROTOCOLO_ESTADO_LONGITUD];
        size_t longitud;
        bool decodifica;
        bool convierte;
        estado_sistema_t esperado;
        uint16_t secuencia;
    } casos[] = {
        {"precaucion", {1, 1, 1, 0x4E, 0x2A, 0}, 6, true, true,
         {MODO_EN_MOVIMIENTO, RIESGO_PRECAUCION, 78}, 42},
        {"seguro", {1, 1, 0, 0x32, 1, 0}, 6, true, true,
         {MODO_EN_MOVIMIENTO, RIESGO_SEGURO, 50}, 1},
        {"detenido", {1, 2, 0, 0x64, 0xFF, 0xFF}, 6, true, true,
         {MODO_DETENIDO, RIESGO_SEGURO, 100}, 65535},
        {"sin datos", {1, 0, 0, 0, 0, 0}, 6, true, true,
         {MODO_SIN_DATOS, RIESGO_SEGURO, 0}, 0},
        {"longitud incorrecta", {1, 1, 1, 78, 42, 0}, 5, false, false, {0}, 0},
        {"version incorrecta", {2, 1, 1, 78, 42, 0}, 6, true, false, {0}, 0},
        {"modo invalido", {1, 99, 1, 78, 42, 0}, 6, true, false, {0}, 0},
        {"riesgo invalido", {1, 1, 99, 78, 42, 0}, 6, true, false, {0}, 0},
        {"nivel movimiento >100", {1, 1, 1, 101, 42, 0}, 6, true, false, {0}, 0},
        {"nivel detenido >100", {1, 2, 0, 101, 42, 0}, 6, true, false, {0}, 0},
        {"detenido ignora riesgo", {1, 2, 255, 100, 0x34, 0x12}, 6, true, true,
         {MODO_DETENIDO, RIESGO_SEGURO, 100}, 0x1234},
        {"sin datos ignora campos", {1, 0, 255, 255, 0, 0}, 6, true, true,
         {MODO_SIN_DATOS, RIESGO_SEGURO, 0}, 0},
    };
    unsigned aprobados = 0;
    const unsigned total = sizeof(casos) / sizeof(casos[0]);
    for (unsigned i = 0; i < total; i++) {
        paquete_espnow_t paquete = {91, 92, 93, 94, 0xA55A};
        estado_sistema_t estado = {MODO_DETENIDO, RIESGO_PELIGRO, 37};
        bool decodifica = paquete_decodificar(casos[i].bytes, casos[i].longitud, &paquete);
        bool ok = decodifica == casos[i].decodifica;
        if (!decodifica) {
            // Comparar campos evita depender del padding de las estructuras.
            ok = ok && paquete.version == 91 && paquete.modo == 92 &&
                 paquete.riesgo == 93 && paquete.nivel_agua_pct == 94 &&
                 paquete.secuencia == 0xA55A;
        } else {
            ok = ok && paquete.version == casos[i].bytes[0] &&
                 paquete.modo == casos[i].bytes[1] && paquete.riesgo == casos[i].bytes[2] &&
                 paquete.nivel_agua_pct == casos[i].bytes[3];
            bool convierte = paquete_a_estado(&paquete, &estado);
            ok = ok && convierte == casos[i].convierte;
            if (convierte) {
                ok = ok && estado.modo == casos[i].esperado.modo &&
                     estado.riesgo == casos[i].esperado.riesgo &&
                     estado.nivel_agua_pct == casos[i].esperado.nivel_agua_pct &&
                     paquete.secuencia == casos[i].secuencia;
                ESP_LOGI("prueba_protocolo", "%s: version=%u modo_wire=%u riesgo_wire=%u nivel=%u secuencia=%u",
                         casos[i].nombre, (unsigned)paquete.version, (unsigned)paquete.modo,
                         (unsigned)paquete.riesgo, (unsigned)paquete.nivel_agua_pct,
                         (unsigned)paquete.secuencia);
            } else {
                ok = ok && estado.modo == MODO_DETENIDO &&
                     estado.riesgo == RIESGO_PELIGRO && estado.nivel_agua_pct == 37;
            }
        }
        if (ok) {
            aprobados++;
            ESP_LOGI("prueba_protocolo", "PASS: %s", casos[i].nombre);
        } else {
            ESP_LOGE("prueba_protocolo", "FAIL: %s", casos[i].nombre);
        }
    }
    ESP_LOGI("prueba_protocolo", "Resultado: %u/%u pruebas PASS", aprobados, total);
}

#endif

#if !USAR_ESPNOW
static unsigned paso;
static int64_t proximo_paso_us;
static const uint32_t duraciones_ms[] = {2000, 3000, 3000, 3000, 4000, 3000};
static const estado_sistema_t estados_simulados[] = {
    { MODO_EN_MOVIMIENTO, RIESGO_SEGURO,     78 },
    { MODO_EN_MOVIMIENTO, RIESGO_PRECAUCION, 78 },
    { MODO_EN_MOVIMIENTO, RIESGO_PELIGRO,    78 },
    { MODO_DETENIDO,      RIESGO_SEGURO,     78 },
    { MODO_SIN_DATOS,     RIESGO_SEGURO,      0 }
};
#endif

static bool estado_sistema_valido(const estado_sistema_t *estado)
{
    if (estado == NULL) return false;

    switch (estado->modo) {
    case MODO_EN_MOVIMIENTO:
        return estado->riesgo == RIESGO_SEGURO ||
               estado->riesgo == RIESGO_PRECAUCION ||
               estado->riesgo == RIESGO_PELIGRO;
    case MODO_DETENIDO:
        return estado->nivel_agua_pct <= 100;
    case MODO_SIN_DATOS:
        return true;
    default:
        return false;
    }
}

// Llamar bajo el bloqueo LVGL existente.
static void mostrar_estado_sistema(const estado_sistema_t *estado)
{
    if (!estado_sistema_valido(estado)) {
        interfaz_mostrar_sin_datos();
        return;
    }

    switch (estado->modo) {
    case MODO_EN_MOVIMIENTO:
        interfaz_mostrar_riesgo(estado->riesgo);
        break;
    case MODO_DETENIDO:
        interfaz_mostrar_nivel(estado->nivel_agua_pct);
        break;
    case MODO_SIN_DATOS:
        interfaz_mostrar_sin_datos();
        break;
    }
}

#if !USAR_ESPNOW
static void mostrar_paso_simulado(void)
{
    if (paso == 0) {
        interfaz_mostrar_inicio();
    } else {
        mostrar_estado_sistema(&estados_simulados[paso - 1]);
    }
}

// Ejecutado por lv_timer_handler(), bajo el bloqueo LVGL existente.
static void simulacion_cb(lv_timer_t *timer)
{
    (void)timer;
    bool confirmado = actualizar_fase_local();
    if (fase != FASE_OPERATIVA) return;
    int64_t ahora = esp_timer_get_time();
    if (confirmado || ahora >= proximo_paso_us) {
        // El arranque es local: la simulacion operativa recorre solo pasos 1..5.
        paso = confirmado ? 1 : (paso % 5) + 1;
        mostrar_paso_simulado();
        proximo_paso_us = ahora + (int64_t)duraciones_ms[paso] * 1000;
    }
}
#else
// PENDIENTE: sustituir por la MAC STA real del emisor antes de la prueba.
// Todo ceros es un marcador; no habilita recepcion desde cualquier emisor.
static const uint8_t MAC_EMISOR_STA[6] = {
    0x8C, 0x94, 0xDF, 0x48, 0xE2, 0x60
};

static void tarea_recepcion(void *argumento)
{
    (void)argumento;
    receptor_estado_init();
    bool mostrado = false;
    estado_sistema_t anterior = {MODO_SIN_DATOS, RIESGO_SEGURO, 0};
    for (;;) {
        espnow_recepcion_t recepcion;
        if (espnow_receptor_recibir(&recepcion, pdMS_TO_TICKS(50)) &&
            memcmp(recepcion.mac_origen, MAC_EMISOR_STA, sizeof(MAC_EMISOR_STA)) == 0 &&
            recepcion.longitud_original == PROTOCOLO_ESTADO_LONGITUD) {
            paquete_espnow_t paquete;
            if (paquete_decodificar(recepcion.datos, recepcion.longitud_original, &paquete)) {
                bool aceptado = receptor_estado_procesar(&paquete);
#if PRUEBA_RTT
                if (aceptado) (void)espnow_receptor_rtt_ack(paquete.secuencia);
#else
                (void)aceptado;
#endif
            }
        }
        receptor_estado_actualizar_timeout();
        display_lock();
        bool confirmado = actualizar_fase_local();
        display_unlock();
        if (fase != FASE_OPERATIVA) continue;
        if (confirmado) mostrado = false;
        estado_sistema_t actual;
        if (receptor_estado_obtener(&actual)) {
            bool cambio = !mostrado || actual.modo != anterior.modo ||
                (actual.modo == MODO_EN_MOVIMIENTO && actual.riesgo != anterior.riesgo) ||
                (actual.modo == MODO_DETENIDO && actual.nivel_agua_pct != anterior.nivel_agua_pct);
            if (cambio) {
                display_lock();
                mostrar_estado_sistema(&actual);
                display_unlock();
                anterior = actual;
                mostrado = true;
            }
        }
    }
}
#endif

void app_main(void)
{
#if EJECUTAR_PRUEBAS_TEMPORALES
    probar_protocolo();
    probar_receptor();
#endif
    lv_display_t *display = display_init();
    preparar_boton_seleccion();

    ESP_LOGI("example", "Display LVGL Meter Widget");
    display_lock();
    interfaz_init(display);
    interfaz_mostrar_calibracion(15);
    inicio_calibracion_us = esp_timer_get_time();
#if !USAR_ESPNOW
    paso = 0;
    lv_timer_t *timer = lv_timer_create(simulacion_cb, 50, NULL);
    LV_ASSERT_MALLOC(timer);
#endif
    display_unlock();
#if USAR_ESPNOW
    ESP_ERROR_CHECK(espnow_receptor_init(1));
#if PRUEBA_RTT
    ESP_ERROR_CHECK(espnow_receptor_rtt_peer(MAC_EMISOR_STA));
#endif
    const uint8_t mac_pendiente[6] = {0};
    if (memcmp(MAC_EMISOR_STA, mac_pendiente, sizeof(mac_pendiente)) == 0) {
        ESP_LOGW("receptor", "Configure MAC_EMISOR_STA en main.c; emisor pendiente");
    }
    ESP_LOGI("receptor", "ESP-NOW canal 1; esperando datos validos");
    if (xTaskCreate(tarea_recepcion, "recepcion", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
#endif
}
