#include "simulation.h"

#include <limits.h>

enum {
    KMH_TO_MPS_NUM = 1000,
    KMH_TO_MPS_DEN = 3600
};

typedef struct {
    double ego_speed;
    double front_speed;
    double initial_distance;
    double reaction_time;
    double ego_deceleration;
    double front_deceleration;
    double road_factor;
} TrialParams;

typedef struct {
    double ego_pos;
    double front_pos;
    double ego_speed;
    double front_speed;
} VehicleState;

static volatile unsigned int workload_sink;

static double clamp_nonnegative(double value)
{
    return value < 0.0 ? 0.0 : value;
}

static unsigned int lcg_next(unsigned int *state)
{
    *state = (*state * 1664525u + 1013904223u);
    return *state;
}

static double rand_double(unsigned int *state, double min, double max)
{
    unsigned int v = lcg_next(state);
    double unit = (double)v / (double)UINT_MAX;
    return min + (max - min) * unit;
}

static TrialParams trial_params_generate(unsigned int *seed)
{
    const double kmh_to_mps = (double)KMH_TO_MPS_NUM / (double)KMH_TO_MPS_DEN;
    TrialParams params;
    params.ego_speed = rand_double(seed, 60.0, 120.0) * kmh_to_mps;
    params.front_speed = rand_double(seed, 40.0, 110.0) * kmh_to_mps;
    params.initial_distance = rand_double(seed, 5.0, 100.0);
    params.reaction_time = rand_double(seed, 0.5, 2.5);
    params.ego_deceleration = rand_double(seed, 3.0, 9.0);
    params.front_deceleration = rand_double(seed, 2.0, 10.0);
    params.road_factor = rand_double(seed, 0.5, 1.0);
    return params;
}

static VehicleState vehicle_state_init(const TrialParams *params)
{
    VehicleState state;
    state.ego_pos = 0.0;
    state.front_pos = params->initial_distance;
    state.ego_speed = params->ego_speed;
    state.front_speed = params->front_speed;
    return state;
}

static void advance_one_step(const TrialParams *params, VehicleState *state,
                             double t, double dt)
{
    state->front_speed = clamp_nonnegative(
        state->front_speed - params->front_deceleration * dt);
    state->front_pos += state->front_speed * dt;
    if (t >= params->reaction_time) {
        state->ego_speed = clamp_nonnegative(
            state->ego_speed - params->ego_deceleration *
            params->road_factor * dt);
    }
    state->ego_pos += state->ego_speed * dt;
}

static double compute_ttc(const VehicleState *state, double large_ttc)
{
    const double epsilon = 1.0e-9;
    double relative_distance = state->front_pos - state->ego_pos;
    double relative_speed = state->ego_speed - state->front_speed;
    if (relative_speed > epsilon && relative_distance > 0.0) {
        return relative_distance / relative_speed;
    }
    return large_ttc;
}

static RiskLevel classify_risk(int collided, double min_ttc)
{
    if (collided) return RISK_COLLISION;
    if (min_ttc < 1.5) return RISK_HIGH;
    if (min_ttc < 3.0) return RISK_MEDIUM;
    if (min_ttc < 5.0) return RISK_LOW;
    return RISK_SAFE;
}

unsigned int simulation_seed_for_trial(unsigned int base_seed, long trial_index)
{
    return base_seed ^ ((unsigned int)trial_index * 2654435761u);
}

RiskLevel run_trial(unsigned int *seed, int time_steps, int *collided_out)
{
    const double dt = 0.1;
    const double large_ttc = 1.0e9;
    TrialParams params = trial_params_generate(seed);
    VehicleState state = vehicle_state_init(&params);
    double min_ttc = large_ttc;
    int collided = 0;

    for (int step = 0; step < time_steps; ++step) {
        double t = (double)step * dt;
        double ttc;
        advance_one_step(&params, &state, t, dt);
        ttc = compute_ttc(&state, large_ttc);
        if (ttc < min_ttc) {
            min_ttc = ttc;
        }
        if (state.front_pos - state.ego_pos <= 0.0) {
            collided = 1;
            break;
        }
    }

    if (collided_out != 0) {
        *collided_out = collided;
    }
    return classify_risk(collided, min_ttc);
}

static int workload_difficulty_for_trial(const Config *cfg, long trial_index)
{
    long heavy_start;
    if (cfg == 0 || cfg->workload_mode == WORKLOAD_UNIFORM) {
        return 0;
    }
    heavy_start = cfg->trials - (cfg->trials / 4);
    if (trial_index >= heavy_start) {
        return cfg->skew_factor;
    }
    return 0;
}

static void burn_cpu_work(int difficulty, unsigned int seed)
{
    unsigned int x = seed;
    int iterations = difficulty * 16;
    for (int i = 0; i < iterations; ++i) {
        x = x * 1664525u + 1013904223u;
        x ^= x >> 13;
    }
    workload_sink = x;
}

static long long profile_inner_work(const Config *cfg)
{
    if (cfg == 0) {
        return 0;
    }
    if (cfg->inner_work > 0) {
        return cfg->inner_work;
    }
    switch (cfg->profile) {
    case PROFILE_PROCESS_FRIENDLY:
        return 2000;
    case PROFILE_THREAD_FRIENDLY:
        return 64;
    case PROFILE_DEFAULT:
    default:
        return 0;
    }
}

static void burn_inner_cpu_work(long long inner_work, unsigned int seed)
{
    volatile unsigned long long x = seed;
    for (long long i = 0; i < inner_work; ++i) {
        x = x * 1664525ULL + 1013904223ULL;
        x ^= (x >> 13);
        x += (unsigned long long)i;
    }
    workload_sink = (unsigned int)x;
}

RiskLevel run_trial_for_index(const Config *cfg, long trial_index,
                              int *collided_out)
{
    unsigned int trial_seed;
    int difficulty;
    long long inner_work;
    if (cfg == 0) {
        if (collided_out != 0) *collided_out = 0;
        return RISK_SAFE;
    }
    trial_seed = simulation_seed_for_trial(cfg->seed, trial_index);
    difficulty = workload_difficulty_for_trial(cfg, trial_index);
    if (difficulty > 0) {
        burn_cpu_work(difficulty, trial_seed);
    }
    inner_work = profile_inner_work(cfg);
    if (inner_work > 0) {
        burn_inner_cpu_work(inner_work, trial_seed);
    }
    return run_trial(&trial_seed, cfg->time_steps, collided_out);
}

void run_batch(const Config *cfg, const TaskBatch *batch, Result *out)
{
    result_init(out);
    for (long i = batch->start_idx; i < batch->end_idx; ++i) {
        int collided = 0;
        RiskLevel risk = run_trial_for_index(cfg, i, &collided);
        result_add_trial(out, risk, collided);
    }
    out->checksum = result_compute_checksum(out);
}
