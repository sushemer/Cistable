#include "seleccion.h"
#include "display.h"
#include "interfaz.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "sdkconfig.h"
static bool done,enabled,candidate,stable,pressed;
static int64_t changed,started;
static modelo_camion_t selected=CAMION_1;
void seleccion_init(void) {
#ifdef CONFIG_CI_ENABLE_BUTTON
    const gpio_config_t c={.pin_bit_mask=1ULL<<CONFIG_CI_BUTTON_GPIO,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    gpio_config(&c);
#else
    done=true; /* No fabricated button input; explicit default Model A logged by app. */
#endif
    display_lock();
    if(!done) interfaz_mostrar_seleccion(selected);
    display_unlock();
}
bool seleccion_modelo_a(void) {return selected==CAMION_1;}
bool seleccion_actualizar(void) {
    if(done) return true;
    int64_t now=esp_timer_get_time();bool down=gpio_get_level(CONFIG_CI_BUTTON_GPIO)==0;
    if(down!=candidate) {candidate=down;changed=now;}
    if(now-changed<30000) return false;
    if(!enabled) {if(!down) {enabled=true;stable=false;}return false;}
    if(stable!=down) {
        stable=down;
        if(down) {pressed=true;started=changed;}
        else if(pressed) {
            pressed=false;selected=selected==CAMION_1?CAMION_2:CAMION_1;
            display_lock();interfaz_mostrar_seleccion(selected);display_unlock();
        }
    }
    if(pressed&&now-started>=1000000) {
        pressed=false;done=true;display_lock();interfaz_confirmar_seleccion(selected);display_unlock();
    }
    return done;
}
