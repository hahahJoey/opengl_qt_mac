//
// Created by zhaojunhe on 2018/8/10.
//

#include <iostream>
#include <time.h>
#include <math.h>
#include <QMatrix4x4>
#include "04_shadowmap.hpp"
#include "../../lib/loadshader/LoadShader.hpp"

#define FRUSTUM_DEPTH       800.0f
#define DEPTH_TEXTURE_SIZE  2048

ShadowMapGLWidget::ShadowMapGLWidget(QWidget *parent) : QOpenGLWidget(parent) {

}

ShadowMapGLWidget::~ShadowMapGLWidget() {
    makeCurrent();
    glUseProgram(0);
    glDeleteProgram(render_light_prog);
    glDeleteProgram(render_scene_prog);
    glDeleteBuffers(1, &ground_vbo);
    glDeleteVertexArrays(1, &ground_vao);
    doneCurrent();
}

void ShadowMapGLWidget::initializeGL() {
    std::cout << __FUNCTION__ << std::endl;
    glCreateProgram();

    ShaderInfo light_shaders[] = {
            {GL_VERTEX_SHADER,   "/Users/junhe/Documents/OpenGL/opengl_qt_mac/vermilion/04_shadowmap/shadow_vs.glsl"},
            {GL_FRAGMENT_SHADER, "/Users/junhe/Documents/OpenGL/opengl_qt_mac/vermilion/04_shadowmap/shadow_fs.glsl"},
            {GL_NONE}
    };

    render_light_prog = LoadShaders(light_shaders);
    render_light_uniforms.model_view_projection_matrix = glGetUniformLocation(render_light_prog,
                                                                              "model_view_projection_matrix");

    // Create the program for rendering the scene from the viewer's position
    ShaderInfo scene_shaders[] = {
            {GL_VERTEX_SHADER,   "/Users/junhe/Documents/OpenGL/opengl_qt_mac/vermilion/04_shadowmap/scene_vs.glsl"},
            {GL_FRAGMENT_SHADER, "/Users/junhe/Documents/OpenGL/opengl_qt_mac/vermilion/04_shadowmap/scene_fs.glsl"},
            {GL_NONE}
    };

    render_scene_prog = LoadShaders(scene_shaders);
    // Get the locations of all the uniforms in the program
    render_scene_uniforms.model_matrix = glGetUniformLocation(render_scene_prog, "model_matrix");
    render_scene_uniforms.view_matrix = glGetUniformLocation(render_scene_prog, "view_matrix");
    render_scene_uniforms.projection_matrix = glGetUniformLocation(render_scene_prog, "projection_matrix");
    render_scene_uniforms.shadow_matrix = glGetUniformLocation(render_scene_prog, "shadow_matrix");
    render_scene_uniforms.light_position = glGetUniformLocation(render_scene_prog, "light_position");
    render_scene_uniforms.material_ambient = glGetUniformLocation(render_scene_prog, "material_ambient");
    render_scene_uniforms.material_diffuse = glGetUniformLocation(render_scene_prog, "material_diffuse");
    render_scene_uniforms.material_specular = glGetUniformLocation(render_scene_prog, "material_specular");
    render_scene_uniforms.material_specular_power = glGetUniformLocation(render_scene_prog, "material_specular_power");

    glUseProgram(render_scene_prog);
    glUniform1i(glGetUniformLocation(render_scene_prog, "depth_texture"), 0);

    // Create a depth texture
    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, DEPTH_TEXTURE_SIZE, DEPTH_TEXTURE_SIZE, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create GBO to render depth into
    glGenFramebuffers(1, &depth_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, depth_texture, 0);
    glDrawBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Upload geometry for the ground plane
    static const float ground_vertices[] = {
            -500.0f, -50.0f, -500.0f, 1.0f,
            -500.0f, -50.0f, 500.0f, 1.0f,
            500.0f, -50.0f, 500.0f, 1.0f,
            500.0f, -50.0f, -500.0f, 1.0f,
    };

    static const float ground_normals[] = {
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &ground_vao);
    glGenBuffers(1, &ground_vbo);
    glBindVertexArray(ground_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ground_vertices) + sizeof(ground_normals), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ground_vertices), ground_vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(ground_vertices), sizeof(ground_normals), ground_normals);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *) sizeof(ground_vertices));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Load the object
    object.LoadFromVBM("/Users/junhe/Documents/OpenGL/opengl_qt_mac/vermilion/04_shadowmap/armadillo_low.vbm", 0, 1, 2);
}

void ShadowMapGLWidget::resizeGL(int w, int h) {
    std::cout << __FUNCTION__ << std::endl;
    current_width = w;
    current_height = h;

    aspect = float(w) / float(h);
}

void ShadowMapGLWidget::paintGL() {
    std::cout << __FUNCTION__ << std::endl;

    static float q = 0.0f;
    static const QVector3D X(1.0f, 0.0f, 0.0f);
    static const QVector3D Y(0.0f, 1.0f, 0.0f);
    static const QVector3D Z(0.0f, 0.0f, 1.0f);

    QVector3D light_position = QVector3D(sinf(0.5 * 6.0f * 3.141592f) * 300.0f, 200.0f,
                                         cosf(0.5 * 4.0f * 3.141592f) * 100.0f + 250.0f);

    // Setup
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Matrices for rendering the scene
    QMatrix4x4 scene_model_matrix;
    scene_model_matrix.setToIdentity();
    scene_model_matrix.rotate(0.5 * 720.0f, Y);
    QMatrix4x4 scene_view_matrix;
    scene_view_matrix.setToIdentity();
    scene_view_matrix.translate(0.0f, 0.0f, -300.0f);
    QMatrix4x4 scene_projection_matrix;
    scene_projection_matrix.setToIdentity();
    scene_projection_matrix.frustum(-1.0f, 1.0f, -aspect, aspect, 1.0f, FRUSTUM_DEPTH);

    const QMatrix4x4 scale_bias_matrix = QMatrix4x4(0.5f, 0.0f, 0.0f, 0.0f,
                                                    0.0f, 0.5f, 0.0f, 0.0f,
                                                    0.0f, 0.0f, 0.5f, 0.0f,
                                                    0.5f, 0.5f, 0.5f, 1.0f);

    // Matrices used when rendering from the light's position
    QMatrix4x4 light_view_matrix;
    light_view_matrix.setToIdentity();
    light_view_matrix.lookAt(light_position, QVector3D(0.0f, 0.0f, 0.0f), Y);
    QMatrix4x4 light_projection_matrix;
    light_projection_matrix.setToIdentity();
    light_projection_matrix.frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, FRUSTUM_DEPTH);

    // Now we render from the light's position into the depth buffer.
    // Select the appropriate program
    glUseProgram(render_light_prog);
    glUniformMatrix4fv(render_light_uniforms.model_view_projection_matrix, 1, GL_FALSE,
                       (light_projection_matrix * light_view_matrix * scene_model_matrix).data());

    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
    glViewport(0, 0, DEPTH_TEXTURE_SIZE, DEPTH_TEXTURE_SIZE);

    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0, 4.0);

    DrawScene(true);
    glDisable(GL_POLYGON_OFFSET_FILL);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, current_width, current_height);

    // Now render from the viewer's position
    glUseProgram(render_scene_prog);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    // Setup all the matrices
    glUniformMatrix4fv(render_scene_uniforms.model_matrix, 1, GL_FALSE, scene_model_matrix.data());
    glUniformMatrix4fv(render_scene_uniforms.view_matrix, 1, GL_FALSE, scene_view_matrix.data());
    glUniformMatrix4fv(render_scene_uniforms.projection_matrix, 1, GL_FALSE, scene_projection_matrix.data());
    glUniformMatrix4fv(render_scene_uniforms.shadow_matrix, 1, GL_FALSE,
                       (scale_bias_matrix * light_projection_matrix * light_view_matrix).data());
    GLfloat lightPos[3] = {light_position.x(), light_position.y(), light_position.z()};
    glUniform3fv(render_scene_uniforms.light_position, 1, lightPos);

    // Bind the depth texture
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Draw
    DrawScene(false);

}

void ShadowMapGLWidget::DrawScene(bool depth_only) {
    // Set material properties for the object
    if (!depth_only) {
        GLfloat v1[] = {0.1f, 0.0f, 0.2f};
        GLfloat v2[] = {0.3f, 0.2f, 0.8f};
        GLfloat v3[] = {1.0f, 1.0f, 1.0f};
        glUniform3fv(render_scene_uniforms.material_ambient, 1, v1);
        glUniform3fv(render_scene_uniforms.material_diffuse, 1, v2);
        glUniform3fv(render_scene_uniforms.material_specular, 1, v3);
        glUniform1f(render_scene_uniforms.material_specular_power, 25.0f);
    }

    // Draw the object
    object.Render();

    // Set material properties for the ground
    if (!depth_only) {
        GLfloat v1[] = {0.1f, 0.1f, 0.1f};
        GLfloat v2[] = {0.1f, 0.5f, 0.1f};
        GLfloat v3[] = {0.1f, 0.1f, 0.1f};
        glUniform3fv(render_scene_uniforms.material_ambient, 1, v1);
        glUniform3fv(render_scene_uniforms.material_diffuse, 1, v2);
        glUniform3fv(render_scene_uniforms.material_specular, 1, v3);
        glUniform1f(render_scene_uniforms.material_specular_power, 3.0f);
    }

    // Draw the ground
    glBindVertexArray(ground_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}


void ShadowMapGLWidget::keyPressEvent(QKeyEvent *event) {
}
