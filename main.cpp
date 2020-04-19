#include <QSurfaceFormat>
#include <QtDebug>
#include <QOpenGLWidget>
#include <QGraphicsView>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

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

#include "CocoaGLContext.h"
//------------------------------------------------------------------------------

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    RenderWidget() : QOpenGLWidget()
    {
    }

    virtual ~RenderWidget() {
        // Wait until all rendered operations are completed before we destroy
        // anything.
        filament::Fence::waitAndDestroy(mEngine->createFence());

        mEngine->destroy(mMainCamera);
        mEngine->destroy(mView);
        mEngine->destroy(mRenderer);
        mEngine->destroy(mSwapChain);

        // Make sure we clean up the swap chain before killing the engine.
        mEngine->flushAndWait();

        filament::Engine::destroy(&mEngine);

        delete m_program;
    }

    void renderFilament()
    {
        if (mRenderer->beginFrame(mSwapChain)) {
            mRenderer->render(mView);
            mRenderer->endFrame();
        }
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

        if (!mEngine) {
            void *nativewindow = reinterpret_cast<void*>(winId());

            // create texture for render target in qt's opengl context
            glGenTextures(1, &m_col_texture_id);
            glBindTexture(GL_TEXTURE_2D, m_col_texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 100, 100, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            QVariant ctx = context()->nativeHandle();
            void *sharedContext = GetCocoaGLContext(ctx);

            qInfo() << "shared context: " << sharedContext;

            auto backend = filament::Engine::Backend::OPENGL;
            mEngine = filament::Engine::create(backend, nullptr, sharedContext);
            mSwapChain = mEngine->createSwapChain(nativewindow);
            mRenderer = mEngine->createRenderer();

            filament::Texture* tex_col = filament::Texture::Builder()
                .width(100)
                .height(100)
                .levels(1)
                .usage(filament::Texture::Usage::COLOR_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE)
                .format(filament::Texture::InternalFormat::RGB8)
                .import(m_col_texture_id)
                .build(*mEngine);

            filament::RenderTarget* target = filament::RenderTarget::Builder()
                .texture(filament::RenderTarget::COLOR, tex_col)
                //.texture(filament::RenderTarget::DEPTH, tex_depth)
                .build(*mEngine);

            mMainCamera = mEngine->createCamera();
            mScene = mEngine->createScene();
            mView = mEngine->createView();

            mView->setClearColor({0.125, 0.125, 0.25, 0.0});
            mView->setScene(mScene);

            // mView->setViewport({0, 0, static_cast<uint32_t>(m_viewportSize.width()),
            //                     static_cast<uint32_t>(m_viewportSize.height())});
            mView->setViewport({0, 0, 100, 100});
            mView->setRenderTarget(target);
            mView->setCamera(mMainCamera);

            // Flush the back buffer
            while (!mRenderer->beginFrame(mSwapChain));
            mRenderer->render(mView);
            mRenderer->endFrame();
        }
    }

protected:
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vertex;
    QOpenGLVertexArrayObject m_object;

    unsigned int m_col_texture_id;
    unsigned int m_quad_vao;
    unsigned int m_quad_vbo;

    filament::Engine* mEngine = nullptr;
    filament::SwapChain* mSwapChain = nullptr;
    filament::Renderer* mRenderer = nullptr;
    filament::Scene* mScene = nullptr;
    filament::View* mView = nullptr;
    filament::Camera *mMainCamera = nullptr;
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
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override
    {
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
    view.show();

    scene->addText("Hello World");

    return app.exec();
}
//! [1]
