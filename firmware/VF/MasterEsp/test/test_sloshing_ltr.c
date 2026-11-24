#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../main/modules/ltr/ltr_module.h"
#include "../main/modules/pendulum/pendulum_module.h"
#include "../../shared/network_protocol.h"

static void test_ltr(void)
{
    ltr_config_t config = {
        .cg_height_m = 1.5f,
        .track_width_m = 1.6f,
        .gravity_m_s2 = 9.80665f,
        .minimum_value = -1.0f,
        .maximum_value = 1.0f,
    };
    ltr_result_t result;

    assert(ltr_calculate(&config, 0.0f, 0.0f, &result));
    assert(fabsf(result.value) < 1e-6f);
    assert(ltr_calculate(&config, 10.0f, 0.0f, &result));
    assert(isfinite(result.value));
    assert(ltr_calculate(&config, 90.0f, 100.0f, &result));
    assert(result.value <= 1.0f && result.value >= -1.0f);
}

static void test_trammel(void)
{
    trammel_params_t params;
    assert(!pendulum_compute_trammel_params(-0.1f, 1.10f, 0.62f, 3000.0f, &params));
    assert(!pendulum_compute_trammel_params(1.2f, 1.10f, 0.62f, 3000.0f, &params));

    float peak = -1.0f;
    float peak_fill = 0.0f;
    const float fills[] = {0.0f, 0.1f, 0.5f, 0.6f, 0.75f, 1.0f};
    for (size_t i = 0; i < sizeof(fills)/sizeof(fills[0]); ++i) {
        const bool valid = pendulum_compute_trammel_params(
            fills[i], 1.10f, 0.62f, 3000.0f, &params);
        if (fills[i] < 0.05f || fills[i] > 0.95f) {
            assert(!valid);
            continue;
        }
        assert(valid);
        assert(params.is_valid);
        assert(isfinite(params.bp_m) && isfinite(params.ap_m));
        assert(params.bp_m > 0.0f && params.ap_m > 0.0f);
        assert(params.mp_kg >= 0.0f && params.mp_kg <= 3000.0f);
        assert(params.omega_n_rad_s > 0.0f);
        const float ratio = params.mp_kg / 3000.0f;
        if (ratio > peak) {
            peak = ratio;
            peak_fill = fills[i];
        }
    }
    assert(isfinite(peak));
    assert(peak_fill >= 0.05f && peak_fill <= 0.95f);
}

static void test_integration(void)
{
    const float dt_values[] = {0.001f, 0.010f, 0.020f};
    for (size_t i = 0; i < sizeof(dt_values)/sizeof(dt_values[0]); ++i) {
        const float dt = dt_values[i];
        pendulum_model_t model = {
            .mass_kg = 100.0f,
            .pivot_to_cg_m = 0.5f,
            .gravity_m_s2 = 9.80665f,
            .damping_coefficient = 2.0f * 0.07f * 4.429f,
        };
        pendulum_state_t state = {.angle_rad = 0.05f};
        float previous_energy = 100.0f;
        for (int step = 0; step < 500; ++step) {
            assert(pendulum_simulate_vehicle_step(&model, &state,
                                                  25.0f, 0.0f,
                                                  4.0f * (step < 10), dt));
            assert(isfinite(state.angle_rad));
            assert(isfinite(state.angular_rate_rad_s));
            const float energy = state.angular_rate_rad_s *
                                     state.angular_rate_rad_s +
                                 9.80665f * state.angle_rad *
                                     state.angle_rad;
            assert(isfinite(energy));
            if (step > 100) assert(energy <= previous_energy + 0.01f);
            previous_energy = energy;
        }
    }
}

static void test_protocol(void)
{
    network_tank_config_payload_t config = {
        .config_version = NETWORK_TANK_CONFIG_VERSION,
        .vehicle_track_m = 1.60f,
        .semi_axis_a_m = 1.10f,
        .semi_axis_b_m = 0.62f,
        .tank_length_m = 3.04f,
        .cg_height_empty_m = 1.20f,
        .cg_height_full_m = 1.50f,
        .full_tank_value = 120.0f,
        .liquid_density_kg_m3 = 1000.0f,
        .damping_ratio_lat = 0.07f,
        .damping_ratio_long = 0.07f,
        .slosh_enable_pct = 10.0f,
        .slosh_disable_low_pct = 5.0f,
        .slosh_disable_high_pct = 75.0f,
    };
    uint8_t packet[128];
    size_t written = 0;
    assert(network_packet_encode(NETWORK_HEADER_AP, NETWORK_CMD_TANK_CONFIG,
                                 &config, sizeof(config),
                                 packet, sizeof(packet), &written));
    assert(written == sizeof(config) + 4U);
    assert(network_packet_validate(packet, written));

    network_tank_config_payload_t decoded;
    memcpy(&decoded, &packet[3], sizeof(decoded));
    assert(decoded.config_version == NETWORK_TANK_CONFIG_VERSION);
    assert(decoded.damping_ratio_lat == 0.07f);
}

int main(void)
{
    test_ltr();
    test_trammel();
    test_integration();
    test_protocol();
    puts("sloshing/ltr tests: OK");
    return 0;
}
