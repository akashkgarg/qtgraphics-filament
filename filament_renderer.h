#pragma once

#include <QString>
#include <QColor>
#include <QImage>

#include <memory>

#include <assimp/scene.h>

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

#include "camera.h"

//------------------------------------------------------------------------------

class FilamentRenderer {
public:
    // Explicitly deleted default constructor
    FilamentRenderer() = default;
    // Explicitly defaulted virtual destructor
    virtual ~FilamentRenderer();

    void init(void* nativewindow, void *sharedContext,
              int width, int height, unsigned int col_texture_id);

    void resetRootTransform();

    void setScene(const aiScene *scene, std::string filename);

    virtual void draw();

    virtual void resize(uint32_t w, uint32_t h);

    void set_projection(uint32_t w, uint32_t h);

private:
    float mFOV = 30.f;

    float mZDist = 1.0f;
    float mRotX = 0.0f;
    float mRotY = 0.0f;

    struct RenderMesh {
        filament::VertexBuffer *vb;
        filament::IndexBuffer *ib;
        filament::Box aabb;
        uint32_t indexCount;
    };

    struct MatTextures {
        QString albedoFilepath;
        filament::Texture *albedo = nullptr;
        filament::Texture *normalMap = nullptr;
        filament::Texture *aoMap = nullptr;
        filament::Texture *specMap = nullptr;
        filament::Texture *maskMap = nullptr;
    };

    const aiScene *mAiScene = nullptr;

    filament::Engine* mEngine = nullptr;
    filament::SwapChain* mSwapChain = nullptr;
    filament::Renderer* mRenderer = nullptr;
    filament::Camera* mMainCamera = nullptr;
    filament::Scene* mScene = nullptr;
    filament::View* mView = nullptr;
    filament::Texture *mRenderTexture = nullptr;
    utils::Entity mLight;

    utils::Entity mRoot;
    utils::Entity mCenterNode; // holds xform to move geo to origin

    filament::Material *mMaterial = nullptr;

    CameraManipulator mCamManipulator;

    std::vector<MatTextures> mTextures;
    std::vector<filament::MaterialInstance*> mMaterialInstances;
    std::vector<RenderMesh> mRenderMeshes;
    std::vector<utils::Entity> mRenderables;

    void createRenderMesh(const aiScene *scene, aiMesh const *mesh);
    void createRenderables(const aiScene *scene,
                           aiNode const *node,
                           aiMatrix4x4 transform);
    void createMaterials(const aiScene *scene, const aiMaterial *mat,
                         const std::string &basedir);
    QImage createOneByOneImage(QImage::Format format, const QColor &color);
    filament::Texture* createTexture(QString img_path,
                                     QColor default_color,
                                     QImage::Format format,
                                     filament::Texture::InternalFormat tex_format);
    void centerCamera();

    void cleanupTextures();
    void cleanupMaterials();
    void cleanupRenderMeshes();
    void cleanupRenderables();
    void cleanupRenderElements();
};
