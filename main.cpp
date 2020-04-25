#include <QSurfaceFormat>
#include <QtDebug>
#include <QOpenGLWidget>
#include <QGraphicsView>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filament/Texture.h>
#include <utils/Entity.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament/RenderTarget.h>
#include <filament/Fence.h>

#include "filament_renderer.h"
#include "CocoaGLContext.h"
//------------------------------------------------------------------------------

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    RenderWidget() : QOpenGLWidget()
    {
    }

    virtual ~RenderWidget() {
        delete m_filament_renderer;

        delete m_program;
    }

    void loadFile(const std::string &pFile)
    {
        using namespace Assimp;
        // Create an instance of the Importer class
        mImporter = std::make_unique<Importer>();

        // And have it read the given file with some example postprocessing
        // Usually - if speed is not the most important aspect for you - you'll
        // probably to request more postprocessing than we do in this example.
        const aiScene* scene = mImporter->ReadFile( pFile,
                                                    aiProcess_CalcTangentSpace       |
                                                    aiProcess_Triangulate            |
                                                    aiProcess_JoinIdenticalVertices  |
                                                    aiProcess_FixInfacingNormals     |
                                                    aiProcess_SortByPType);

        // If the import failed, report it
        if( !scene)
        {
            qInfo() << "Failed to load scene";
            return;
        }
        qInfo() << "Loaded scene successfully";

        qInfo() << "Num meshes: " << scene->mNumMeshes;

        m_filament_renderer->setScene(scene, pFile);
    }

    void renderFilament()
    {
        m_filament_renderer->draw();
    }

    void renderFilamentTexture()
    {
        makeCurrent();

        glClearColor(0.25, 0.25, 0.25, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_program->bind();
        m_program->setUniformValue("screenTexture", (int)0);
        m_object.bind();
        glBindTexture(GL_TEXTURE_2D, m_col_texture_id);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_object.release();

        m_program->release();

        doneCurrent();
    }

protected:
    void initializeGL() override {
        QOpenGLWidget::initializeGL();
        initializeOpenGLFunctions();
        glClearColor(1.0, 0.0, 0.0, 1.0);
        qInfo() << "initialized gl";

        float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
            // positions   // texCoords
            -0.5f,  0.5f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.0f, 0.0f,
            0.5f, -0.5f,  1.0f, 0.0f,

            -0.5f,  0.5f,  0.0f, 1.0f,
            0.5f, -0.5f,  1.0f, 0.0f,
            0.5f,  0.5f,  1.0f, 1.0f
        };

        if (!m_program)
        {
            m_program = new QOpenGLShaderProgram();
            m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               R"SHADER(#version 330 core
                                                       layout (location = 0) in vec2 aPos;
                                                       layout (location = 1) in vec2 aTexCoords;
                                                       out vec2 TexCoords;
                                                       void main()
                                                       {
                                                           TexCoords = aTexCoords;
                                                           gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
                                                       })SHADER");
            m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                               R"SHADER(#version 330 core
                                                       out vec4 FragColor;
                                                       in vec2 TexCoords;
                                                       uniform sampler2D screenTexture;
                                                       void main()
                                                       {
                                                           vec3 col = texture(screenTexture, TexCoords).rgb;
                                                           FragColor = vec4(col, 1.0);
                                                       })SHADER");

            m_program->link();
            m_program->bind();

            // create vbo
            m_vertex.create();
            m_vertex.bind();
            m_vertex.setUsagePattern(QOpenGLBuffer::StaticDraw);
            m_vertex.allocate(quadVertices, sizeof(quadVertices));

            // create vao
            m_object.create();
            m_object.bind();
            m_program->enableAttributeArray(0);
            m_program->enableAttributeArray(1);
            m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, sizeof(float)*4);
            m_program->setAttributeBuffer(1, GL_FLOAT, 2*sizeof(float), 2, sizeof(float)*4);

            // Release (unbind) all
            m_object.release();
            m_vertex.release();
            m_program->release();
        }

        if (!m_filament_renderer) {
            void *nativewindow = reinterpret_cast<void*>(winId());

            int render_dim = 256;

            // create texture for render target in qt's opengl context
            glGenTextures(1, &m_col_texture_id);
            glBindTexture(GL_TEXTURE_2D, m_col_texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_dim, render_dim, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            QVariant ctx = context()->nativeHandle();
            void *sharedContext = GetCocoaGLContext(ctx);

            qInfo() << "shared context: " << sharedContext;

            m_filament_renderer = new FilamentRenderer();
            m_filament_renderer->init(nativewindow, sharedContext, render_dim, render_dim, m_col_texture_id);

            m_filament_renderer->resize(width(), height());
        }
    }

protected:
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vertex;
    QOpenGLVertexArrayObject m_object;

    unsigned int m_col_texture_id;
    unsigned int m_quad_vao;
    unsigned int m_quad_vbo;

    FilamentRenderer *m_filament_renderer = nullptr;

    // The asset scene importer.
    std::unique_ptr<Assimp::Importer> mImporter;
};

//------------------------------------------------------------------------------

class RenderScene : public QGraphicsScene
{
public:
    RenderScene(RenderWidget *renderwidget)
        : m_render_widget(renderwidget)
    {
        qInfo() << "created scene";
    }

protected:
    virtual void drawBackground(QPainter *painter, const QRectF &) override
    {
        qInfo() << "rendering background";
        painter->beginNativePainting();
        m_render_widget->renderFilament();
        m_render_widget->renderFilamentTexture();
        painter->endNativePainting();
    }

    RenderWidget *m_render_widget = nullptr;
};

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    QSurfaceFormat::setDefaultFormat(format);

    RenderWidget *rg = new RenderWidget();
    RenderScene *scene = new RenderScene(rg);
    QGraphicsView view;
    view.setViewport(rg);
    view.setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view.setScene(scene);
    view.resize(600, 600);
    scene->addText("Hello World");
    view.show();

    if (argc == 2) {
        rg->loadFile(argv[1]);
    }

    return app.exec();
}
//! [1]
