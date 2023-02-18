#define _CRT_SECURE_NO_WARNINGS
#include "snail.cpp"
#include "cow.cpp"
#include "_cow_supplement.cpp"
#include "jim.cpp"

using namespace std;

double h = .02;   // simulation timestep
double L = 1;     // pendulum length
double g = -9.81; // gravitational acceleration
double m = 1;     // mass (of point at end of pendulum)

vec2 hw9a_get_p(double theta)
{
    // position of the pendulum's mass (end point)
    // convention: theta = 0 <-> straight down
    return L * V2(sin(theta), -cos(theta));
}

double hw9a_get_alpha(double theta)
{
    // TODO angular acceleration
    double inertia = m * L * L;
    double torque = m * g * L * sin(theta);
    return (torque / inertia);
};

double hw9a_get_PE(double theta)
{
    // TODO potential energy
    vec2 position = hw9a_get_p(theta);
    return (-m * g * position.y);
}
double hw9a_get_KE(double omega)
{
    // TODO kinetic energy
    // omega is angular velocity
    return ((m * (L * omega) * (L * omega)) / 2.0);
}
double hw9a_get_E(double theta, double omega)
{
    // total energy
    return hw9a_get_PE(theta) + hw9a_get_KE(omega);
};

void hw9a()
{
    init();

    Camera2D camera = {5.35, 0, -.75};
    bool paused = false;

    double theta = RAD(90);
    double omega = 0;

    const int TRACE_LENGTH = 128;
    StretchyBuffer<vec2> position_trace = {};
    vec3 trace_colors[TRACE_LENGTH];
    {
        for_(node_i, TRACE_LENGTH)
        {
            trace_colors[node_i] = color_plasma(NUM_DENm1(node_i, TRACE_LENGTH));
        }
    }

    enum Mode
    {
        MODE_EXPLICIT,
        MODE_SEMI_IMPLICIT,
        MODE_IMPLICIT,
        _NUM_MODES
    };
    int mode = 0;
    char *modes[_NUM_MODES];
    modes[MODE_EXPLICIT] = "explicit euler";
    modes[MODE_SEMI_IMPLICIT] = "semi implicit euler";
    modes[MODE_IMPLICIT] = "implicit euler";

    struct
    {
        bool hide_plots;
    } tweaks = {};

    while (begin_frame())
    {
        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);
        gl_PV(PV);

        { // gui
            imgui_slider("h", &h, .001, .1);
            imgui_slider("L", &L, .1, 5);
            imgui_slider("m", &m, .1, 10);
            imgui_slider("g", &g, -24, 24);
            imgui_readout("theta", &theta);
            imgui_readout("omega", &omega);
            { // mode
                imgui_slider("mode", &mode, 0, _NUM_MODES - 1, 'j', 'k', true);
                _imgui_printf("mode: %s", modes[mode]);
            }
            imgui_checkbox("paused", &paused, 'p');
            { // reset
                static double _theta_0 = theta;
                static double _omega_0 = omega;
                if (imgui_button("reset", 'r'))
                {
                    theta = _theta_0;
                    omega = _omega_0;
                    sbuff_free(&position_trace);
                }
            }
            imgui_checkbox("hide_plots", &tweaks.hide_plots, 'b');
        }

        if (!paused)
        {
            // TODO step simulation state forward in time
            // NOTE please use hw9a_get_alpha(theta) here
            if (mode == MODE_EXPLICIT)
            {
                // TODO explicit euler (be careful!)
                double alpha = hw9a_get_alpha(theta);
                theta += h * omega;
                omega += h * alpha;
            }
            else if (mode == MODE_SEMI_IMPLICIT)
            {
                double alpha = hw9a_get_alpha(theta);
                omega += h * alpha;
                theta += h * omega;
            }
            else if (mode == MODE_IMPLICIT)
            {
                vec2 state_k = V2(theta, omega); // current state
                vec2 est_kp1 = state_k;          // estimate of state k+1
                while (true)
                {
                    vec2 est_curr = est_kp1;                                                         // current estimate
                    est_kp1 = V2(state_k.x + h * est_kp1.y, state_k.y + h * g / L * sin(est_kp1.x)); // update estimate
                    if (IS_EQUAL(0.0, squaredNorm(est_curr - est_kp1)))                              // if estimates stopped changing
                    {
                        break; // est_k is approximately state k+1
                    }
                }
                theta = est_kp1.x;
                omega = est_kp1.y;
            }
        }

        vec2 p = hw9a_get_p(theta);

        { // drag
            if (widget_drag(PV, 1, &p))
            {
                p = L * normalized(p);
                omega = 0;
                theta = RAD(90) + atan2(p.y, p.x);
            }
        }

        {     // draw
            { // position_trace
                if (position_trace.length < TRACE_LENGTH)
                {
                    sbuff_push_back(&position_trace, p);
                }
                else
                {
                    memmove(position_trace.data, position_trace.data + 1, (TRACE_LENGTH - 1) * sizeof(vec2));
                    position_trace.data[TRACE_LENGTH - 1] = p;
                }
                basic_draw(LINE_STRIP, PV, position_trace.length, position_trace.data, trace_colors);
            }
            { // pendulum
                gl_color(monokai.white);
                gl_begin(LINES);
                gl_vertex(V2(0, 0));
                gl_vertex(p);
                gl_end();
                gl_begin(POINTS);
                gl_vertex(p);
                gl_end();
            }
            { // energy plot
                if (!tweaks.hide_plots)
                {
                    mat4 PV_plot = Translation(.3, -.3) * Scaling(.5, .5 * window_get_aspect());
                    {
                        vec2 axes[] = {{1, 0}, {0, 0}, {0, 1}};
                        basic_draw(LINE_STRIP, PV_plot, 3, axes, monokai.gray);
                        basic_text(PV_plot, "time", axes[0]);
                        basic_text(PV_plot, "energy", axes[2], V3(1, 1, 1), 0, {0, -24});
                    }
                    {
                        static vec2 PE_trace[TRACE_LENGTH];
                        static vec2 KE_trace[TRACE_LENGTH];
                        static vec2 E_trace[TRACE_LENGTH];
                        do_once{
                            for_(i, TRACE_LENGTH){
                                E_trace[i].x = KE_trace[i].x = PE_trace[i].x = NUM_DENm1(i, TRACE_LENGTH);
                    }
                };
                if (!paused)
                {
                    for_(i, TRACE_LENGTH - 1)
                    {
                        PE_trace[i].y = PE_trace[i + 1].y;
                        KE_trace[i].y = KE_trace[i + 1].y;
                        E_trace[i].y = E_trace[i + 1].y;
                    }
                    PE_trace[TRACE_LENGTH - 1].y = hw9a_get_PE(theta);
                    KE_trace[TRACE_LENGTH - 1].y = hw9a_get_KE(omega);
                    E_trace[TRACE_LENGTH - 1].y = hw9a_get_E(theta, omega);
                }
                mat4 S = Scaling(1, .05);
                basic_draw(LINE_STRIP, PV_plot * S, TRACE_LENGTH, PE_trace, monokai.blue);
                basic_draw(LINE_STRIP, PV_plot * S, TRACE_LENGTH, KE_trace, monokai.red);
                basic_draw(LINE_STRIP, PV_plot * S, TRACE_LENGTH, E_trace, monokai.purple);
            }
        }
    }
}
}
}

void hw9b()
{
    init();

    // // skeleton
    // bone_lengths          -- the length of the bones
    // bone_relative_angles  -- the angle between a given bone and the next bone
    // bone_rest_positions   -- the origin of each bone in the initial pose
    // * NOTE You may assume that the rest positions all lie along the x-axis.
    // skeleton_total_length -- the lenth of the entire skeleton in its rest pose
    const int NUM_BONES = 4;
    double bone_lengths[NUM_BONES];
    double bone_relative_angles[NUM_BONES] = {};
    vec2 bone_rest_positions[NUM_BONES + 1] = {};
    double skeleton_total_length = 0;
    {
        for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
        {
            bone_lengths[bone_i] = LERP(double(bone_i) / MAX(1, NUM_BONES - 1), 2.5, 1);
            bone_rest_positions[bone_i + 1] = bone_rest_positions[bone_i] + V2(bone_lengths[bone_i], 0);
        }
        skeleton_total_length = bone_rest_positions[NUM_BONES].x;
    }

    enum Mode
    {
        MODE_RIGID,
        MODE_SMOOTH,
        _NUM_MODES,
    };
    char *modes[_NUM_MODES];
    modes[MODE_RIGID] = "rigid";
    modes[MODE_SMOOTH] = "smooth";

    // // skin
    // node_rest_positions -- the positions of all nodes in the rest pose
    // weights             -- for a given skinning mode, the binding weights of each node
    const int NUM_NODES = 64;
    STATIC_ASSERT(NUM_NODES % 2 == 0);
    vec2 node_rest_positions[NUM_NODES] = {};
    double weights[_NUM_MODES][NUM_NODES][NUM_BONES] = {}; // weights[mode][node_i][bone_i]
    {
        {
            // rest pose as a loop
            // e.g., for 10 nodes:
            // 9 8 7 6 5
            // 0 1 2 3 4
            int k = 0;
            for (int sign = -1; sign <= 1; sign += 2)
            {
                for (int node_i = 0; node_i < NUM_NODES / 2; ++node_i)
                {
                    double f = NUM_DENm1(node_i, NUM_NODES / 2);
                    if (sign > 0)
                        f = 1 - f;
                    node_rest_positions[k++] = V2(skeleton_total_length * f, .3 * sign);
                }
            }
        }

        // weights
        for (int node_i = 0; node_i < NUM_NODES / 2; ++node_i)
        {
            for_(mode, _NUM_MODES)
            {
                // calculate the node_i-th node's weights (not normlized)
                double w[NUM_BONES] = {};
                {

                    if (mode == MODE_RIGID)
                    {
                        // idea: bind the node entirely to the closest bone
                        // NOTE I did this for you
                        double X_i = node_rest_positions[node_i].x;
                        for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
                        {
                            if (X_i < bone_rest_positions[bone_i + 1].x + TINY)
                            {
                                w[bone_i] = 1;
                                break;
                            }
                        }
                    }

                    else if (mode == MODE_SMOOTH)
                    {
                        // idea: smoothly blend the weights between nearby bones
                        // (many possible ways to implement this)
                        // TODO (harder; we will cover one approach in Thursday's class)

                        for (int j = 0; j < NUM_BONES; j++)
                        {
                            // bone length = bone_rest_positions[j] - bone_rest_positions[j+1]; L left R right
                            // a and b for the window
                            // a < L && b > L: weights[mode][i][j] += b-L
                            // a > L && b < R: wij += b-a
                            // a < R && b > R: wij += r-a
                            double a = node_rest_positions[node_i - 1].x;
                            double b = node_rest_positions[node_i + 1].x;
                            double L = bone_rest_positions[j].x;
                            double R = bone_rest_positions[j + 1].x;
                            if (a < L - TINY && b > L - TINY)
                            {
                                w[j] = b - L;
                            }
                            if (a > L - TINY && b < R + TINY)
                            {
                                w[j] = b - a;
                            }
                            if (a < R + TINY && b > R + TINY)
                            {
                                w[j] = R - a;
                            }
                        }
                    }
                }

                { // normalize
                    double sum_w = 0;
                    {
                        for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
                        {
                            sum_w += w[bone_i];
                        }
                        for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
                        {
                            w[bone_i] /= sum_w;
                        }
                    }
                }

                { // write w into 3D array
                    for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
                    {
                        weights[mode][node_i][bone_i] = weights[mode][NUM_NODES - 1 - node_i][bone_i] = w[bone_i];
                    }
                }
            }
        }
    }

    struct
    {
        bool draw_character;
        bool hide_rig;
        bool hide_nodes;
        bool draw_rest_pose;
        bool hide_plots;
    } tweaks = {};

    // debug play
    bool debug_play = false;
    double debug_time = 0;

    // keyframing system
    const int MAX_KEYFRAMES = 1024;
    double keyframes[MAX_KEYFRAMES][NUM_BONES] = {};
    int num_keyframes = 0;
    bool tween_keyframes = false;
    const int FRAMES_PER_KEYFRAME = 32;
    int frame = 0;

    Camera2D camera = {10, 0, 2};
    int mode = 0;

    while (begin_frame())
    {
        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);
        gl_PV(PV);

        { // gui
            for_(bone_i, NUM_BONES)
            {
                char buffer[24];
                sprintf(buffer, "theta_%d", bone_i);
                imgui_slider(buffer, bone_relative_angles + bone_i, -PI, PI, true);
            }
            imgui_slider("mode", &mode, 0, _NUM_MODES - 1, 'j', 'k', true);
            _imgui_printf("mode: %s", modes[mode]);
            if (!tweaks.hide_plots)
            {
                mat4 PV_plot = Translation(0, .25) * Scaling(.59, .5 * .5 * window_get_aspect());
                static vec2 trace[NUM_NODES / 2];
                {
                    vec2 axes[] = {{1, 0}, {0, 0}, {0, 1}};
                    basic_draw(LINE_STRIP, PV_plot, 3, axes, monokai.gray);
                    basic_text(PV_plot, "x_rest", axes[0]);
                    basic_text(PV_plot, "weight", axes[2], V3(1, 1, 1), 0, {0, -24});
                }
                for_(bone_i, NUM_BONES)
                {
                    for_(node_i, NUM_NODES / 2)
                    {
                        trace[node_i] = {NUM_DENm1(node_i, NUM_NODES / 2), weights[mode][node_i][bone_i]};
                    }
                    basic_draw(LINE_STRIP, PV_plot, NUM_NODES / 2, trace, color_get_kelly(bone_i));
                }
            }
            imgui_checkbox("debug_play", &debug_play, 'p');
            if (imgui_button("save keyframe", 's') && num_keyframes < MAX_KEYFRAMES)
            {
                for_(bone_i, NUM_BONES)
                {
                    keyframes[num_keyframes][bone_i] = bone_relative_angles[bone_i];
                }
                ++num_keyframes;
            }
            imgui_checkbox("tween keyframes", &tween_keyframes, 't');
            imgui_readout("num_keyframes", &num_keyframes);
            { // reset
                static double _bone_relative_angles_0[NUM_BONES];
                do_once { memcpy(_bone_relative_angles_0, bone_relative_angles, sizeof(bone_relative_angles)); };
                if (imgui_button("reset", 'r'))
                {
                    memcpy(bone_relative_angles, _bone_relative_angles_0, sizeof(bone_relative_angles));

                    debug_time = 0;

                    memset(keyframes, 0, sizeof(keyframes));
                    num_keyframes = 0;
                    tween_keyframes = false;
                    frame = 0;
                }
            }
            imgui_checkbox("draw_character", &tweaks.draw_character, 'z');
            imgui_checkbox("draw_rest_pose", &tweaks.draw_rest_pose, 'v');
            imgui_checkbox("hide_rig", &tweaks.hide_rig, 'x');
            imgui_checkbox("hide_nodes", &tweaks.hide_nodes, 'c');
            imgui_checkbox("hide_plots", &tweaks.hide_plots, 'b');
        }

        // aniamte the skeleton (i.e., set bone_relative_angles using whatever method you like)
        // METHOD 1: press 'p' to play a sinusoidal trajectory
        // METHOD 2: press 's' to save keyframes; press 't' to play them back with lerp
        {
            if (tween_keyframes && num_keyframes > 0)
            {
                int A = (frame / FRAMES_PER_KEYFRAME) % num_keyframes;
                int B = (A + 1) % num_keyframes;
                double t = double(frame % FRAMES_PER_KEYFRAME) / FRAMES_PER_KEYFRAME;
                for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
                {
                    bone_relative_angles[bone_i] = LERP(
                        t,
                        keyframes[A][bone_i],
                        keyframes[B][bone_i]);
                }
                ++frame;
            }
            else if (debug_play)
            {
                for (int bone_i = 1; bone_i < NUM_BONES; ++bone_i)
                {
                    bone_relative_angles[bone_i] += .02 * cos((bone_i + 1) * debug_time / 2);
                }
                debug_time += .0167;
            }
        }

        // bone_absolute_angles -- the angle of each bone in world coordinates
        // NOTE feel free to use these for forward kinematics and skinning!
        double bone_absolute_angles[NUM_BONES] = {};
        {
            for (int bone_i = 0; bone_i < NUM_BONES; ++bone_i)
            {
                if (bone_i > 0)
                {
                    bone_absolute_angles[bone_i] = bone_absolute_angles[bone_i - 1];
                }
                bone_absolute_angles[bone_i] += bone_relative_angles[bone_i];
            }
        }

        // // forward kinematics
        // TODO compute bone_current_positions (the skeleton will show up)
        vec2 bone_current_positions[NUM_BONES + 1] = {};
        {
            for (int i = 0; i < NUM_BONES; i++)
            {
                bone_current_positions[i + 1] = bone_current_positions[i] + bone_lengths[i] * V2(cos(bone_absolute_angles[i]), sin(bone_absolute_angles[i]));
            }
        }

        // // skinning
        // TODO compute node_curr_positions (the skin will show up)
        vec2 node_curr_positions[NUM_NODES] = {};
        {
            for (int i = 0; i < NUM_NODES; i++)
            {
                for (int j = 0; j < NUM_BONES; j++)
                {

                    vec2 p = node_rest_positions[i] - bone_rest_positions[j];
                    mat2 R = M2(cos(bone_absolute_angles[j]), -sin(bone_absolute_angles[j]), sin(bone_absolute_angles[j]), cos(bone_absolute_angles[j]));
                    vec2 T = bone_current_positions[j];
                    vec2 q = R * p + T;
                    node_curr_positions[i] += weights[mode][i][j] * q;
                }
            }
        }

        { // draw
            if (!tweaks.hide_rig)
            { // draw
                gl_begin(GL_LINES);
                {
                    for_(bone_i, NUM_BONES)
                    {
                        gl_color(color_get_kelly(bone_i));
                        gl_vertex(bone_current_positions[bone_i]);
                        gl_vertex(bone_current_positions[bone_i + 1]);
                    }
                }
                gl_end();
                basic_draw(POINTS, PV, NUM_BONES + 1, bone_current_positions);
            }

            if (tweaks.draw_rest_pose)
            {
                basic_draw(POINTS, PV, NUM_BONES + 1, bone_rest_positions, monokai.blue);
                basic_draw(LINE_STRIP, PV, NUM_BONES + 1, bone_rest_positions, monokai.blue);
                basic_draw(LINE_LOOP, PV, NUM_NODES, node_rest_positions, .5 * monokai.blue);
                basic_draw(POINTS, PV, NUM_NODES, node_rest_positions, monokai.blue, 4);
            }

            if (!tweaks.hide_nodes)
            {
                basic_draw(LINE_LOOP, PV, NUM_NODES, node_curr_positions, monokai.gray);
                basic_draw(POINTS, PV, NUM_NODES, node_curr_positions, monokai.white, 4);
            }

            if (tweaks.draw_character)
            {
                const int NUM_QUADS = (NUM_NODES / 2 - 1);
                static vec2 vertex_positions[4 * NUM_QUADS];
                static vec3 vertex_colors[4 * NUM_QUADS];
                for_(node_i, NUM_QUADS)
                {
                    vertex_positions[4 * node_i + 0] = node_curr_positions[node_i];
                    vertex_positions[4 * node_i + 1] = node_curr_positions[node_i + 1];
                    vertex_positions[4 * node_i + 2] = node_curr_positions[NUM_NODES - 1 - (node_i + 1)];
                    vertex_positions[4 * node_i + 3] = node_curr_positions[NUM_NODES - 1 - (node_i)];
                    double f = double(node_i) / (NUM_QUADS - 1);
                    vec3 color = color_rainbow_swirl(f);
                    vertex_colors[4 * node_i + 0] = color;
                    vertex_colors[4 * node_i + 1] = color;
                    vertex_colors[4 * node_i + 2] = color;
                    vertex_colors[4 * node_i + 3] = color;
                }
                basic_draw(QUADS, PV, 4 * NUM_QUADS, vertex_positions, vertex_colors);
            }
        }
    }
}

double random_color()
{
    return rand() % 256;
}

double random_position()
{
    return rand() % 100;
}

double random_velocity()
{
    return rand() % 6 - 3;
}

void hw9c()
{
    init();
    vec2 *boids_positions = (vec2 *)calloc(100, sizeof(vec2));
    for (int p = 0; p < 100; p++)
    {
        boids_positions[p] = {random_position(), random_position()};
    }

    vec3 *boids_colors = (vec3 *)calloc(100, sizeof(vec3));
    for (int c = 0; c < 100; c++)
    {
        boids_colors[c] = {random_color() / 255, random_color() / 255, random_color() / 255};
    }
    vec2 *boids_velocity = (vec2 *)calloc(100, sizeof(vec2));
    for (int v = 0; v < 100; v++)
    {
        boids_velocity[v] = {random_velocity(), random_velocity()};
    }

    vec2 *boids_test = (vec2 *)calloc(100, sizeof(vec2));
    for (int v = 0; v < 100; v++)
    {
        boids_test[v] = {random_velocity(), random_velocity()};
    }

    while (begin_frame())
    {
        static Camera2D camera = {100, 50, 50};
        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);
        gl_PV(PV);

        vec2 s_mouse = input_get_mouse_position_in_world_coordinates(PV);

        vec2 boids_center = {}; // the center of each boid
        for (int c = 0; c < 100; c++)
        {
            boids_center += boids_positions[c] / 100;
        }

        vec2 vsum = {}; // sum of all velocity
        for (int v = 0; v < 100; v++)
        {
            // if (((boids_center[v].x - boids_positions[v].x) != 0) && ((boids_center[v].y - boids_positions[v].y) != 0))
            //{
            boids_velocity[v] += (boids_center - boids_positions[v]) / 100;
            //}
            vsum += boids_velocity[v];
        }

        // printf("%f", vsum.x);
        // printf("%f", vsum.y);

        vec2 *velocity_match = (vec2 *)calloc(100, sizeof(vec2));

        vec2 *boids_displacement = (vec2 *)calloc(100, sizeof(vec2));

        vec2 *mouse_displacement = (vec2 *)calloc(100, sizeof(vec2));

        for (int i = 0; i < 100; i++)
        {
            for (int j = 0; j < 100; j++)
            {
                if (i != j)
                {
                    if (norm(boids_positions[i] - boids_positions[j]) < 1)
                    {
                        boids_displacement[i] = boids_displacement[i] - (boids_positions[i] - boids_positions[j]);
                    }
                }
            }
            boids_velocity[i] -= boids_displacement[i];

            velocity_match[i] = ((vsum - boids_velocity[i]) / 99) / 8;

            boids_velocity[i] += velocity_match[i];
            // ASSERT(!isnan(boids_velocity[i].x));
            // ASSERT(!isnan(boids_velocity[i].y));

            if (norm(s_mouse - boids_positions[i]) < 6)
            {
                mouse_displacement[i] -= s_mouse - boids_positions[i];
            }

            if (!IS_EQUAL(0., squaredNorm(boids_velocity[i])))
            {
                boids_velocity[i] = CLAMP(norm(boids_velocity[i]), 0.0, 0.3) * normalized(boids_velocity[i]);
                boids_velocity[i] += mouse_displacement[i];
                boids_positions[i] += boids_velocity[i];
            }
        }
        basic_draw(POINTS, PV, 100, boids_positions, boids_colors);

        free(boids_displacement);
        free(velocity_match);
    }
    free(boids_positions);
    free(boids_velocity);
    free(boids_colors);
}

int main()
{
    hw9a();
    hw9b();
    hw9c();
    return 0;
}
