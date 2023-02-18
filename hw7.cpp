#define _CRT_SECURE_NO_WARNINGS
#include "snail.cpp"
#include "cow.cpp"
#include "_cow_supplement.cpp"

void hw7a()
{

    init();

    // mesh
    int num_vertices = 4;
    int num_triangles = 2;
    vec3 vertex_positions[] = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    int3 triangle_indices[] = {{0, 1, 2}, {0, 2, 3}};

    // shaders
    char *vert = R""""(
        #version 330 core
        layout (location = 0) in vec3 _p_model;
        void main() {
            gl_Position = vec4(_p_model, 1);
        }
    )"""";

    char *frag = R""""(
        #version 330 core

        #define LERP(t, a, b) ((1 - (t)) * (a) + (t) * (b))
        #define COS_LERP(t, a, b) LERP(.5 - .5 * cos((t)*3.14), a, b)
        precision highp float;

        uniform float time;

        uniform vec2 iResolution;
        uniform float camera_height;
        uniform vec2 camera_origin;

        uniform int max_iterations; // for mandelbrodt calculation
        uniform int AA; // anti-aliasing

        out vec4 fragColor;

        vec3 plasma(float t) {

        const vec3 c0 = vec3(0.05873234392399702, 0.02333670892565664, 0.5433401826748754);
        const vec3 c1 = vec3(2.176514634195958, 0.2383834171260182, 0.7539604599784036);
        const vec3 c2 = vec3(-2.689460476458034, -7.455851135738909, 3.110799939717086);
        const vec3 c3 = vec3(6.130348345893603, 42.3461881477227, -28.51885465332158);
        const vec3 c4 = vec3(-11.10743619062271, -82.66631109428045, 60.13984767418263);
        const vec3 c5 = vec3(10.02306557647065, 71.41361770095349, -54.07218655560067);
        const vec3 c6 = vec3(-3.658713842777788, -22.93153465461149, 18.19190778539828);

        return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
        }


        void main() {
            vec3 col = vec3(0);
            for (int m = 0; m < AA; m++)
            for (int n = 0; n < AA; n++) {
                vec2 p0 = ((-iResolution.xy+2.0*(gl_FragCoord.xy+vec2(float(m),float(n))/float(AA)))/iResolution.y)*(camera_height/2.0)+camera_origin;
                // NOTE https://en.wikipedia.org/wiki/Plotting_algorithms_for_the_Mandelbrot_set
                // NOTE assume p0 = (x0, y0) is correct (ignore the suggested bounds)           

                //col += vec3(p0.x, p0.y, 0); // TODO add the color of p0 to col

                float x = 0.0;
                float y = 0.0;
                int iteration = 0;

                while (x*x + y*y <= 2*2 && iteration < max_iterations){
                    float xtemp = x*x - y*y + p0[0];
                    y = 2*x*y + p0[1];
                    x = xtemp;
                    iteration = iteration +1;
                }

                if (iteration == max_iterations){
                    col += vec3(0);
                } else {
                    float f = (float(iteration)/float(max_iterations)); //calculating how soon the pixel escapes; if it escapes immediately color it black, if it never escapes color it white (and everything in between)
                    f = (cos(f*10.-time)+1.)/2.;
                    //f *= COS_LERP(time, 0, 1);
                    col += plasma(f);
                    
                }
            }
            col /= (AA * AA);
            fragColor = vec4(col, 1);
        }
    )"""";
    int shader_program = shader_build_program(vert, frag);
    ASSERT(shader_program);

    // misc opengl
    GLuint VAO, VBO, EBO;
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    // misc cow
    bool playing = false;
    Camera2D camera_0 = {4, -.109293, .895227};
    Camera2D camera = camera_0;
    double time = 0;

    struct
    {
        int max_iterations = 256;
        int AA = 1;
    } tweaks;

    while (begin_frame())
    {
        camera_move(&camera);

        { // imgui
            if (imgui_button("reset", 'r'))
            {
                camera = camera_0;
            }
            _imgui_printf("camera_origin = (%lf, %lf)", camera.o_x, camera.o_y);
            imgui_slider("max_iterations", &tweaks.max_iterations, 1, 1024);
            imgui_slider("AA", &tweaks.AA, 1, 5);
            imgui_checkbox("playing", &playing, 'p');
        }

        { // draw
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, num_vertices * 3 * sizeof(double), vertex_positions, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);

            glUseProgram(shader_program);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * num_triangles * sizeof(int), triangle_indices, GL_DYNAMIC_DRAW);

            shader_set_uniform_vec2(shader_program, "iResolution", window_get_dimensions_in_pixels());
            {
                shader_set_uniform_double(shader_program, "camera_height", camera.screen_height_World);
                shader_set_uniform_vec2(shader_program, "camera_origin", &camera.o_x);
                shader_set_uniform_int(shader_program, "max_iterations", tweaks.max_iterations);
                shader_set_uniform_int(shader_program, "AA", tweaks.AA);
            }

            glDrawElements(GL_TRIANGLES, 3 * num_triangles, GL_UNSIGNED_INT, NULL);
        }

        if (playing)
        {
            camera.screen_height_World *= .99;
            shader_set_uniform_double(shader_program, "time", time);
            time += 1. / 60;
        }
    }
}

void hw7b()
{
    init();

    // mesh (grid)
    int S = 64;
    int num_vertices = S * S;
    int num_triangles = (S - 1) * (S - 1) * 2;
    vec3 *vertex_positions = (vec3 *)malloc(num_vertices * sizeof(vec3));
    int3 *triangle_indices = (int3 *)malloc(num_triangles * sizeof(int3));
    {
        { // vertex_positions
            int k = 0;
            for (int j = 0; j < S; ++j)
            {
                for (int i = 0; i < S; ++i)
                {
                    vertex_positions[k++] = {i / double(S - 1), j / double(S - 1), 0};
                }
            }
        }
        { // triangle_indices
            int k = 0;
#define INDEX(i, j) ((j)*S + (i))
            for (int j = 0; j < S - 1; ++j)
            {
                for (int i = 0; i < S - 1; ++i)
                {
                    triangle_indices[k++] = {INDEX(i, j), INDEX(i + 1, j), INDEX(i + 1, j + 1)};
                    triangle_indices[k++] = {INDEX(i, j), INDEX(i + 1, j + 1), INDEX(i, j + 1)};
                }
            }
#undef INDEX
        }
    }

    // shaders
    char *vert = R""""(
        #version 330 core
        layout (location = 0) in vec3 _p_model;
        uniform mat4 PVM;
        out vec2 uv;

        void main() {

            // NOTE https://en.wikipedia.org/wiki/Torus
            vec3 p_model = _p_model; // TODO set p_model
            float theta = p_model.x * 2 * 3.14;
            float phi = p_model.y * 2 * 3.14;

            p_model.x = (10 + 5 * cos(theta))*cos(phi);
            p_model.y = (10 + 5 * cos(theta))*sin(phi);
            p_model.z = 5 * sin(theta);

            gl_Position   = PVM * vec4(p_model, 1);
            uv = _p_model.xy;
        }
    )"""";
    char *frag = R""""(
        #version 330 core
        in vec2 uv;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(.5 - .5 * cos(12 * 6.283186530718 * uv.x), .5 - .5 * cos(12 * 6.283186530718 * uv.y), 1, 1);
        }
    )"""";
    int shader_program = shader_build_program(vert, frag);
    ASSERT(shader_program);

    // misc opengl
    GLuint VAO, VBO, EBO;
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    // misc cow
    bool playing = true;
    Camera3D camera = {3, RAD(45)};

    while (begin_frame())
    {
        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);

        { // draw
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, num_vertices * 3 * sizeof(double), vertex_positions, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);

            glUseProgram(shader_program);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * num_triangles * sizeof(int), triangle_indices, GL_DYNAMIC_DRAW);

            shader_set_uniform_mat4(shader_program, "PVM", PV);

            glDrawElements(GL_TRIANGLES, 3 * num_triangles, GL_UNSIGNED_INT, NULL);
        }

        imgui_checkbox("playing", &playing, 'p');
        if (playing)
        {
        }
    }
}

void hw7c()
{
    init();

    // shaders
    char *vert = R""""(
        #version 330 core

        layout (location = 0) in vec3 _p_model;
        layout (location = 1) in vec3 _n_model;

        uniform mat4 P, V, M;
        uniform float time;

        uniform bool draw_bad_normal_transform;

        out vec3 p_world;
        out vec3 _n_world;

        void main() {
            // NOTE this is inefficient for multiple reasons
            //      i have it the way it is for clarity :)  
            p_world = (M * vec4(_p_model, 1)).xyz;
            _n_world = (transpose(inverse(M)) * vec4(_n_model, 0)).xyz; // correct normal transform
            if (draw_bad_normal_transform) { _n_world = (M * vec4(_n_model, 0)).xyz; } // _incorrect_ normal transform
            gl_Position = P * V * vec4(p_world, 1);
        }
    )"""";
    char *frag = R""""(
        #version 330 core

        out vec4 fragColor;

        uniform bool draw_color_by_normal;

        in vec3 p_world;
        in vec3 _n_world;

        uniform vec3 camera_origin;

        uniform int num_lights;
        uniform vec3 light_positions[16];
        uniform vec3 light_colors[16];

        void main() {

            vec3 n_world = normalize(_n_world); // question: why do we need to normalize?

            vec3 col;
            if (draw_color_by_normal) {
                col = .5 + .5 * _n_world;
            } else {
                col = vec3(.1); // ambient light approximation
                for (int i = 0; i < num_lights; ++i) {
                    vec3 L = normalize(light_positions[i] - p_world);
                    float diff = max(dot(_n_world, light_positions[i]), 0.0);
                    vec3 diffuse = diff * 0.5 * light_colors[i];
                    col = col + diffuse * 0.5;
                    
                    float specularStrength = 0.5;
                    vec3 V = normalize(camera_origin - p_world);
                    //vec3 reflectDir = reflect(-V, _n_world);
                    //float spec = pow(max(dot(V, reflectDir), 0.0), 32);
                    //vec3 specular = specularStrength * spec * light_colors[i];
                    //col = col + specular * 5;

                    vec3 halfwayDir = normalize(L + V);
                    float spec_new = pow(max(dot(_n_world, halfwayDir), 0.0), 32);
                    vec3 new_specular = specularStrength * light_colors[i] * spec_new;
                    col = col + new_specular * 5;

                    // SPOILERS https://learnopengl.com/Advanced-Lighting/Advanced-Lighting

                }
            }
            fragColor = vec4(col, 1);
        }
    )"""";
    int shader_program = shader_build_program(vert, frag);
    ASSERT(shader_program);

    // mesh
    FancyTriangleMesh3D fancy_bunny = load_fancy_mesh("data_fancy_bunny", true, true);
    int mesh_index = 0;
    FancyTriangleMesh3D *meshes[] = {&fancy_bunny, &meshlib.fancy_sphere};

// lights
#define MAX_NUM_LIGHTS 6
    int num_lights = 1;
    vec3 light_positions[MAX_NUM_LIGHTS] = {};
    vec3 light_colors[MAX_NUM_LIGHTS] = {monokai.red, monokai.orange, monokai.yellow, monokai.green, monokai.blue, monokai.purple};
    {
        for (int i = 0; i < MAX_NUM_LIGHTS; ++i)
        {
            light_positions[i] = 3 * normalized(V3(util_random_double(-1, 1), util_random_double(-1, 1), util_random_double(-1, 1)));
        }
        light_positions[0] = {0, 0, 3};
    }
    bool draw_light_positions = true;

    // misc opengl
    GLuint VAO, VBO[2], EBO;
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(2, VBO);
        glGenBuffers(1, &EBO);
    }

    // misc cow
    Camera3D camera = {10, RAD(45)};
    double time = 0;
    bool playing = false;
    bool draw_color_by_normal = false;
    bool draw_bad_normal_transform = false;

    // fornow
    srand(0);

    while (begin_frame())
    {
        camera_move(&camera);
        mat4 P = camera_get_P(&camera);
        mat4 V = camera_get_V(&camera);
        mat4 M = Translation(0, -1, 0) * Scaling(1, 1 + .5 * sin(3 * time), 1) * Translation(0, 1, 0);

        { // imgui and widgets
            imgui_slider("mesh_index", &mesh_index, 0, NELEMS(meshes) - 1, 'h', 'l', true);
            imgui_slider("num_lights", &num_lights, 0, MAX_NUM_LIGHTS, 'j', 'k');
            imgui_checkbox("draw_color_by_normal", &draw_color_by_normal, 'z');
            imgui_checkbox("draw_bad_normal_transform", &draw_bad_normal_transform, 'x');
            imgui_checkbox("draw_light_positions", &draw_light_positions, 'c');
            imgui_checkbox("playing", &playing, 'p');
            if (imgui_button("reset", 'r'))
            {
                time = 0;
            }
            jank_widget_translate3D(P * V, num_lights, light_positions);
        }

        { // draw
            FancyTriangleMesh3D *mesh = meshes[mesh_index];

            if (draw_light_positions)
            {
                basic_draw(POINTS, P * V, num_lights, light_positions, light_colors);
            }
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_positions, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_normals, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(1, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(1);

            glUseProgram(shader_program);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * mesh->num_triangles * sizeof(int), mesh->triangle_indices, GL_DYNAMIC_DRAW);

            shader_set_uniform_mat4(shader_program, "P", P);
            shader_set_uniform_mat4(shader_program, "V", V);
            shader_set_uniform_mat4(shader_program, "M", M);
            shader_set_uniform_double(shader_program, "time", time);

            shader_set_uniform_vec3(shader_program, "camera_origin", camera_get_origin(&camera));
            shader_set_uniform_int(shader_program, "num_lights", num_lights);
            shader_set_uniform_array_vec3(shader_program, "light_positions", num_lights, light_positions);
            shader_set_uniform_array_vec3(shader_program, "light_colors", num_lights, light_colors);
            shader_set_uniform_bool(shader_program, "draw_color_by_normal", draw_color_by_normal);
            shader_set_uniform_bool(shader_program, "draw_bad_normal_transform", draw_bad_normal_transform);

            glDrawElements(GL_TRIANGLES, 3 * mesh->num_triangles, GL_UNSIGNED_INT, NULL);
        }

        if (playing)
        {
            time += 1. / 60;
        }
    }
}

void hw7d()
{
    init();

    // shaders
    char *vert = R""""(
        #version 330 core

        layout (location = 0) in vec3 _p_model;
        layout (location = 1) in vec3 _n_model;

        uniform mat4 P, V, M;
        uniform float time;

        uniform bool draw_bad_normal_transform;

        out vec3 p_world;
        out vec3 _n_world;

        void main() {
            // NOTE this is inefficient for multiple reasons
            //      i have it the way it is for clarity :)  
            p_world = (M * vec4(_p_model, 1)).xyz;
            _n_world = (transpose(inverse(M)) * vec4(_n_model, 0)).xyz; // correct normal transform
            if (draw_bad_normal_transform) { _n_world = (M * vec4(_n_model, 0)).xyz; } // _incorrect_ normal transform
            gl_Position = P * V * vec4(p_world, 1);
        }
    )"""";
    char *frag = R""""(
        #version 330 core

        out vec4 fragColor;

        uniform bool draw_color_by_normal;

        in vec3 p_world;
        in vec3 _n_world;

        uniform vec3 camera_origin;

        uniform int num_lights;
        uniform vec3 light_positions[16];
        uniform vec3 light_colors[16];

        void main() {

            vec3 n_world = normalize(_n_world); // question: why do we need to normalize?

            vec3 col;
            if (draw_color_by_normal) {
                col = .5 + .5 * _n_world;
            } else {
                col = vec3(.1); // ambient light approximation
                for (int i = 0; i < num_lights; ++i) {
                    vec3 L = normalize(light_positions[i] - p_world);
                    float diff = max(dot(_n_world, light_positions[i]), 0.0);
                    vec3 diffuse = diff * 0.5 * light_colors[i];
                    col = col + diffuse * 0.5;
                    
                    float specularStrength = 0.5;
                    vec3 V = normalize(camera_origin - p_world);
                    vec3 reflectDir = reflect(-V, _n_world);
                    float spec = pow(max(dot(V, reflectDir), 0.0), 32);
                    vec3 specular = specularStrength * spec * light_colors[i];
                    col = col + specular * 5;

                    //vec3 halfwayDir = normalize(L + V);
                    //float spec_new = pow(max(dot(_n_world, halfwayDir), 0.0), 32);
                    //vec3 new_specular = specularStrength * light_colors[i] * spec_new;
                    //col = col + new_specular * 5;

                    col = floor(col*10)/10;
                    // SPOILERS https://learnopengl.com/Advanced-Lighting/Advanced-Lighting

                }
            }
            fragColor = vec4(col, 1);
        }
    )"""";
    int shader_program = shader_build_program(vert, frag);
    ASSERT(shader_program);

    // mesh
    FancyTriangleMesh3D fancy_bunny = load_fancy_mesh("data_fancy_bunny", true, true);
    int mesh_index = 0;
    FancyTriangleMesh3D *meshes[] = {&fancy_bunny, &meshlib.fancy_sphere};

// lights
#define MAX_NUM_LIGHTS 6
    int num_lights = 1;
    vec3 light_positions[MAX_NUM_LIGHTS] = {};
    vec3 light_colors[MAX_NUM_LIGHTS] = {monokai.red, monokai.orange, monokai.yellow, monokai.green, monokai.blue, monokai.purple};
    {
        for (int i = 0; i < MAX_NUM_LIGHTS; ++i)
        {
            light_positions[i] = 3 * normalized(V3(util_random_double(-1, 1), util_random_double(-1, 1), util_random_double(-1, 1)));
        }
        light_positions[0] = {0, 0, 3};
    }
    bool draw_light_positions = true;

    // misc opengl
    GLuint VAO, VBO[2], EBO;
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(2, VBO);
        glGenBuffers(1, &EBO);
    }

    // misc cow
    Camera3D camera = {10, RAD(45)};
    double time = 0;
    bool playing = false;
    bool draw_color_by_normal = false;
    bool draw_bad_normal_transform = false;

    // fornow
    srand(0);

    while (begin_frame())
    {
        camera_move(&camera);
        mat4 P = camera_get_P(&camera);
        mat4 V = camera_get_V(&camera);
        mat4 M = Rotation(3.14159 * time) * Translation(time, -time, time) * Translation(0, -1, 0) * Scaling(1, 1 + .5 * sin(3 * time), 1) * Translation(0, 1, 0) * Translation(-time, time, -time);

        { // imgui and widgets
            imgui_slider("mesh_index", &mesh_index, 0, NELEMS(meshes) - 1, 'h', 'l', true);
            imgui_slider("num_lights", &num_lights, 0, MAX_NUM_LIGHTS, 'j', 'k');
            imgui_checkbox("draw_color_by_normal", &draw_color_by_normal, 'z');
            imgui_checkbox("draw_bad_normal_transform", &draw_bad_normal_transform, 'x');
            imgui_checkbox("draw_light_positions", &draw_light_positions, 'c');
            imgui_checkbox("playing", &playing, 'p');
            if (imgui_button("reset", 'r'))
            {
                time = 0;
            }
            jank_widget_translate3D(P * V, num_lights, light_positions);
        }

        { // draw
            FancyTriangleMesh3D *mesh = meshes[mesh_index];

            if (draw_light_positions)
            {
                basic_draw(POINTS, P * V, num_lights, light_positions, light_colors);
            }
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_positions, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_normals, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(1, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(1);

            glUseProgram(shader_program);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * mesh->num_triangles * sizeof(int), mesh->triangle_indices, GL_DYNAMIC_DRAW);

            shader_set_uniform_mat4(shader_program, "P", P);
            shader_set_uniform_mat4(shader_program, "V", V);
            shader_set_uniform_mat4(shader_program, "M", M);
            shader_set_uniform_double(shader_program, "time", time);

            shader_set_uniform_vec3(shader_program, "camera_origin", camera_get_origin(&camera));
            shader_set_uniform_int(shader_program, "num_lights", num_lights);
            shader_set_uniform_array_vec3(shader_program, "light_positions", num_lights, light_positions);
            shader_set_uniform_array_vec3(shader_program, "light_colors", num_lights, light_colors);
            shader_set_uniform_bool(shader_program, "draw_color_by_normal", draw_color_by_normal);
            shader_set_uniform_bool(shader_program, "draw_bad_normal_transform", draw_bad_normal_transform);

            glDrawElements(GL_TRIANGLES, 3 * mesh->num_triangles, GL_UNSIGNED_INT, NULL);
        }

        if (playing)
        {
            time += 1. / 60;
        }
    }
}

int main()
{
    hw7a();
    hw7b();
    hw7c();
    hw7d();
    return 0;
}
