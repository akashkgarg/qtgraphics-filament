#include "filament_renderer.h"
#include "camera.h"

#include <filament/Fence.h>
#include <filament/Camera.h>
#include <utils/EntityManager.h>
#include <utils/Path.h>
#include <filament/TextureSampler.h>
#include <filament/IndirectLight.h>
#include <filament/RenderTarget.h>
#include <math/mat3.h>


#include <QApplication>
#include <QResizeEvent>
#include <QStatusBar>
#include <QtDebug>
#include <QFileInfo>
#include <QImage>
#include <QColor>
#include <QDir>
#include <mutex>
#include "resources/resources.h"

#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>

FilamentRenderer::~FilamentRenderer()
{
    // Wait until all rendered operations are completed before we destroy
    // anything.
    filament::Fence::waitAndDestroy(mEngine->createFence());

    // destroy root entity.
    mEngine->destroy(mCenterNode);
    mEngine->destroy(mRoot);

    cleanupRenderElements();

    mEngine->destroy(mRenderTexture);

    // destroy light and its entity
    mEngine->getLightManager().destroy(mLight);
    mEngine->destroy(mLight);

    // destroy material
    mEngine->destroy(mMaterial);

    mEngine->destroy(mMainCamera);
    mEngine->destroy(mView);
    mEngine->destroy(mRenderer);
    mEngine->destroy(mSwapChain);

    // Make sure we clean up the swap chain before killing the engine.
    mEngine->flushAndWait();

    filament::Engine::destroy(&mEngine);
}

void FilamentRenderer::set_projection(uint32_t w, uint32_t h) {
    // setup projection matrix
    const float aspect = float(w) / h;
    mMainCamera->setProjection(mFOV, aspect, 0.1, 10);
}

void FilamentRenderer::resetRootTransform() {
    using namespace filament;

    // reset object transforms
    mRotX = 0;
    mRotY = 0;
    math::mat4f xform; // identity
    auto& tcm = mEngine->getTransformManager();
    tcm.setTransform(tcm.getInstance(mRoot), xform);

    // reset camera
    mCamManipulator = CameraManipulator({ 0, 0, mZDist},
                                        { 0, 0, 0 },
                                        { 0, 1, 0 });
    mCamManipulator.updateCamera(mMainCamera);
}

void FilamentRenderer::draw() {
    while (!mRenderer->beginFrame(mSwapChain))
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    mRenderer->render(mView);
    mRenderer->endFrame();
    // if (mRenderer->beginFrame(mSwapChain)) {
    //     mRenderer->render(mView);
    //     mRenderer->endFrame();
    // }
}

void FilamentRenderer::resize(uint32_t w, uint32_t h) { set_projection(w, h); }

void FilamentRenderer::init(void* nativewindow, void *sharedContext,
                            int width, int height, unsigned int col_texture_id)
{
    auto backend = filament::Engine::Backend::OPENGL;
    mEngine = filament::Engine::create(backend, nullptr, sharedContext);
    mSwapChain = mEngine->createSwapChain(nullptr);
    mRenderer = mEngine->createRenderer();
    mMainCamera = mEngine->createCamera();
    mScene = mEngine->createScene();
    mView = mEngine->createView();

    filament::Texture* tex_col = filament::Texture::Builder()
        .width(100)
        .height(100)
        .levels(1)
        .usage(filament::Texture::Usage::COLOR_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE)
        .format(filament::Texture::InternalFormat::RGB8)
        .import(col_texture_id)
        .build(*mEngine);
    mRenderTexture = tex_col;

    filament::RenderTarget* target = filament::RenderTarget::Builder()
        .texture(filament::RenderTarget::COLOR, tex_col)
        //.texture(filament::RenderTarget::DEPTH, tex_depth)
        .build(*mEngine);

    mView->setClearColor({1.0, 0.125, 0.25, 0.0});
    mView->setScene(mScene);

    mView->setViewport({0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
    mView->setRenderTarget(target);
    mView->setCamera(mMainCamera);

    set_projection(width, height);

    mLight = utils::EntityManager::get().create();
    filament::LightManager::Builder(filament::LightManager::Type::SUN)
        .color(filament::Color::toLinear<filament::ACCURATE>({0.98f, 0.92f, 0.89f}))
        .intensity(110000)
        .direction({0.6, -0.6, -1.0})
        .castShadows(true)
        .build(*mEngine, mLight);
    mScene->addEntity(mLight);

    // read material.
    mMaterial =
        filament::Material::Builder().package(RESOURCES_TRANSPARENT_DATA, RESOURCES_TRANSPARENT_SIZE)
        .build(*mEngine);

    // Setup the root node to make transforming the object easier.
    mRoot = utils::EntityManager::get().create();
    filament::math::mat4f root_xform =
        filament::math::mat4f::rotation(0, filament::math::float3{0, 1, 0});
    auto& tcm = mEngine->getTransformManager();
    tcm.create(mRoot);
    tcm.setTransform(tcm.getInstance(mRoot), root_xform);

    // flush back buffer
    draw();
}

//------------------------------------------------------------------------------

void FilamentRenderer::createRenderMesh(const aiScene *scene, aiMesh const *mesh)
{
    using namespace filament;
    using namespace filament::math;
    using namespace utils;

    float3 const* positions  = reinterpret_cast<float3 const*>(mesh->mVertices);
    float3 const* tangents   = reinterpret_cast<float3 const*>(mesh->mTangents);
    float3 const* bitangents = reinterpret_cast<float3 const*>(mesh->mBitangents);
    float3 const* normals    = reinterpret_cast<float3 const*>(mesh->mNormals);
    float3 const* texCoords0 = reinterpret_cast<float3 const*>(mesh->mTextureCoords[0]);

    const size_t numVertices = mesh->mNumVertices;

    if (numVertices == 0)
        return;

    const aiFace* faces = mesh->mFaces;
    const size_t numFaces = mesh->mNumFaces;

    if (numFaces == 0)
        return;

    // copy the relevant data.
    float3 *vs = new float3[numVertices];
    float2 *vts = new float2[numVertices];
    float4 *ts = new float4[numVertices];
    for (size_t j = 0; j < numVertices; j++) {
        float3 normal = normals[j];
        float3 tangent;
        float3 bitangent;

        // Assimp always returns 3D tex coords but we only support 2D tex coords.
        float2 texCoord0 = texCoords0 ? texCoords0[j].xy : float2{0.0};
        // If the tangent and bitangent don't exist, make arbitrary ones. This only
        // occurs when the mesh is missing texture coordinates, because assimp
        // computes tangents for us. (search up for aiProcess_CalcTangentSpace)
        if (!tangents) {
            bitangent = normalize(cross(normal, float3{1.0, 0.0, 0.0}));
            tangent = normalize(cross(normal, bitangent));
        } else {
            tangent = tangents[j];
            bitangent = bitangents[j];
        }

        quatf q = filament::math::details::TMat33<float>::packTangentFrame({tangent, bitangent, normal});
        ts[j] = q.xyzw;
        vts[j] = texCoord0;
        vs[j] = positions[j];
    }

    // Populate the index buffer. All faces are triangles at this point because we
    // asked assimp to perform triangulation.
    uint32_t *indices = new uint32_t[numFaces * 3];
    for (size_t j = 0; j < numFaces; ++j) {
        const aiFace& face = faces[j];
        for (size_t k = 0; k < face.mNumIndices; ++k) {
            indices[j*3 + k] = face.mIndices[k];
        }
    }

    auto aabb = RenderableManager::computeAABB(vs, indices, numFaces, sizeof(float3));

    // define the vertex buffer
    auto vb_builder =
        VertexBuffer::Builder()
        .vertexCount(numVertices)
        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3)
        .attribute(VertexAttribute::UV0, 1, VertexBuffer::AttributeType::FLOAT2)
        .attribute(VertexAttribute::TANGENTS, 2, VertexBuffer::AttributeType::FLOAT4)
        .bufferCount(3);
    VertexBuffer *vb = vb_builder.build(*mEngine);

    // copy to gpu
    vb->setBufferAt(*mEngine, 0,
                    VertexBuffer::BufferDescriptor(vs, numVertices * sizeof(float3),
                                                   [](void *buffer, size_t, void*) {
                                                       float3 *data = static_cast<float3*>(buffer);
                                                       delete[] data;
                                                   }));
    vb->setBufferAt(*mEngine, 1,
                    VertexBuffer::BufferDescriptor(vts, numVertices * sizeof(float2),
                                                   [](void *buffer, size_t, void*) {
                                                       float2 *data = static_cast<float2*>(buffer);
                                                       delete[] data;
                                                   }));
    vb->setBufferAt(*mEngine, 2,
                    VertexBuffer::BufferDescriptor(ts, numVertices * sizeof(float4),
                                                   [](void *buffer, size_t, void*) {
                                                       float4 *data = static_cast<float4*>(buffer);
                                                       delete[] data;
                                                   }));
    IndexBuffer *ib =
        IndexBuffer::Builder().indexCount(numFaces * 3)
        .bufferType(IndexBuffer::IndexType::UINT)
        .build(*mEngine);
    ib->setBuffer(*mEngine,
                  IndexBuffer::BufferDescriptor(indices, sizeof(uint32_t) * numFaces * 3,
                                                [](void *buffer, size_t, void*) {
                                                    uint32_t *data = static_cast<uint32_t*>(buffer);
                                                    delete[] data;
                                                }));

    // Keep rendered mesh references for later use
    RenderMesh rm;
    rm.vb = vb;
    rm.ib = ib;
    rm.indexCount = numFaces * 3;

    // compute bounding box
    rm.aabb = aabb;

    // qInfo() << "mesh " << mesh->mName.C_Str() << " min: " << rm.aabb.getMin()[0] << ", " << rm.aabb.getMin()[1] << ", " << rm.aabb.getMin()[2];
    // qInfo() << "mesh " << mesh->mName.C_Str() << " max: " << rm.aabb.getMax()[0] << ", " << rm.aabb.getMax()[1] << ", " << rm.aabb.getMax()[2];
    // qInfo() << "mesh " << mesh->mName.C_Str() << " center " << rm.aabb.center[0] << ", " << rm.aabb.center[1] << ", " << rm.aabb.center[2];

    mRenderMeshes.push_back(rm);
}

//------------------------------------------------------------------------------

QImage FilamentRenderer::createOneByOneImage(QImage::Format format,
                                           const QColor &color)
{
    QImage img(1, 1, format);
    img.setPixelColor(0, 0, color);
    return img;
}

//------------------------------------------------------------------------------

filament::Texture*
FilamentRenderer::createTexture(QString img_path,
                              QColor default_color,
                              QImage::Format format,
                              filament::Texture::InternalFormat tex_format)
{
    using namespace filament;

    if (format != QImage::Format_RGB888 && format != QImage::Format_RGBA8888) {
        qCritical() << "Invalid image format: " << format << ". Falling back to RGB888";
        format = QImage::Format_RGB888;
    }

    QImage img;
    QFileInfo fileinfo(img_path);
    if (!fileinfo.exists()) {
        qInfo() << "File does not exist: " << img_path;
    } else if (!img.load(img_path)) {
        qInfo() << "Could not load image at: " << img_path;
    }

    // ensure correct format
    img = img.convertToFormat(format);

    if (img.isNull()){
        // Create empty texture
        qInfo() << "Creating default texture";
        img = createOneByOneImage(format, default_color);
    }

    unsigned char* img_data = new unsigned char[img.sizeInBytes()];
    memcpy(img_data, img.bits(), img.sizeInBytes());

    Texture::Format pixel_buffer_format = Texture::Format::RGB;
    if (format == QImage::Format_RGB888) {
        pixel_buffer_format = Texture::Format::RGB;
    } else if (format == QImage::Format_RGBA8888) {
        pixel_buffer_format = Texture::Format::RGBA;
    } else {
        qCritical() << "Invalid image format: " << format;
    }

    Texture::PixelBufferDescriptor buffer(img_data,
                                          size_t(img.width() * img.height() * 4),
                                          pixel_buffer_format,
                                          Texture::Type::UBYTE,
                                          [](void* buffer, size_t, void*) {
                                              unsigned char* data =
                                                  static_cast<unsigned char*>(buffer);
                                              delete[] data;
                                          });
    Texture* tex = Texture::Builder()
        .width(uint32_t(img.width()))
        .height(uint32_t(img.height()))
        .levels(1)
        .sampler(Texture::Sampler::SAMPLER_2D)
        .format(tex_format)
        .build(*mEngine);
    tex->setImage(*mEngine, 0, std::move(buffer));

    return tex;
}

//------------------------------------------------------------------------------

void FilamentRenderer::createMaterials(const aiScene *scene,
                                     const aiMaterial *mat,
                                     const std::string &basedir)
{
    using namespace filament;

    qInfo() << "Basedir: " << basedir.c_str();

    QImage img;
    std::string texpath;
    if (mat->GetTextureCount(aiTextureType_DIFFUSE)) {
        aiString tex_path;
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path);

        QString imgpathstr(tex_path.C_Str());
        auto tokens = imgpathstr.split(QRegExp("\\\\|/"));
        qInfo() << tokens;

        //texpath = basedir + "/Textures/" + img_path.fileName().toStdString();
        texpath = basedir + "/Textures/" + tokens.last().toStdString();
    }

    MatTextures textures;

    textures.albedoFilepath = QString(texpath.c_str());
    textures.albedo = createTexture(texpath.c_str(),
                                    Qt::white,
                                    QImage::Format_RGBA8888,
                                    Texture::InternalFormat::SRGB8_A8);

    textures.normalMap = createTexture((basedir + "/Textures/normal.jpg").c_str(),
                                       QColor(127, 127, 255),
                                       QImage::Format_RGB888,
                                       Texture::InternalFormat::RGB8);

    textures.aoMap = createTexture((basedir + "/Textures/ao.jpg").c_str(),
                                   Qt::white,
                                   QImage::Format_RGB888,
                                   Texture::InternalFormat::RGB8);

    textures.specMap = createTexture((basedir + "/Textures/spec.jpg").c_str(),
                                     Qt::black,
                                     QImage::Format_RGB888,
                                     Texture::InternalFormat::RGB8);

    textures.maskMap = createTexture((basedir + "/Textures/mask.jpg").c_str(),
                                     Qt::white,
                                     QImage::Format_RGB888,
                                     Texture::InternalFormat::RGB8);


    mTextures.push_back(textures);

    MaterialInstance *mat_inst = mMaterial->createInstance();

    if (textures.albedo && mat_inst->getMaterial()->hasParameter("albedo")) {
        TextureSampler sampler(TextureSampler::MinFilter::LINEAR,
                               TextureSampler::MagFilter::LINEAR,
                               TextureSampler::WrapMode::REPEAT);  // repeat is needed
        mat_inst->setParameter("albedo", textures.albedo, sampler);
    }

    if (textures.normalMap && mat_inst->getMaterial()->hasParameter("normalMap")) {
        TextureSampler sampler(TextureSampler::MinFilter::LINEAR,
                               TextureSampler::MagFilter::LINEAR,
                               TextureSampler::WrapMode::REPEAT);  // repeat is needed
        mat_inst->setParameter("normalMap", textures.normalMap, sampler);
    }

    if (textures.aoMap && mat_inst->getMaterial()->hasParameter("aoMap")) {
        TextureSampler sampler(TextureSampler::MinFilter::LINEAR,
                               TextureSampler::MagFilter::LINEAR,
                               TextureSampler::WrapMode::REPEAT);  // repeat is needed
        mat_inst->setParameter("aoMap", textures.aoMap, sampler);
    }

    if (textures.specMap && mat_inst->getMaterial()->hasParameter("specMap")) {
        TextureSampler sampler(TextureSampler::MinFilter::LINEAR,
                               TextureSampler::MagFilter::LINEAR,
                               TextureSampler::WrapMode::REPEAT);  // repeat is needed
        mat_inst->setParameter("specMap", textures.specMap, sampler);
    }

    if (textures.maskMap && mat_inst->getMaterial()->hasParameter("maskMap")) {
        TextureSampler sampler(TextureSampler::MinFilter::LINEAR,
                               TextureSampler::MagFilter::LINEAR,
                               TextureSampler::WrapMode::REPEAT);  // repeat is needed
        mat_inst->setParameter("maskMap", textures.maskMap, sampler);
    }

    mMaterialInstances.push_back(mat_inst);
}

//------------------------------------------------------------------------------

void FilamentRenderer::createRenderables(const aiScene *scene,
                                       aiNode const *node,
                                       aiMatrix4x4 accTransform)
{
    using namespace filament;

    aiMatrix4x4 transform = accTransform * node->mTransformation;

    auto &tcm = mEngine->getTransformManager();

    // if the node has meshes, then create renderables
    if (node->mNumMeshes > 0) {

        size_t mesh_idx = node->mMeshes[0];
        size_t mat_idx  = scene->mMeshes[mesh_idx]->mMaterialIndex;

        if (mesh_idx >= mRenderMeshes.size()) {
            qCritical() << "mesh index: " << mesh_idx << " greater than num render meshes: "<< mRenderMeshes.size();
        } else if (mat_idx >= mMaterialInstances.size()) {
            qCritical() << "material index: " << mesh_idx << " greater than num materials: "<< mMaterialInstances.size();
        } else {
            RenderMesh &rm = mRenderMeshes[mesh_idx];
            MaterialInstance *mat = mMaterialInstances[mat_idx];
            utils::Entity renderable = utils::EntityManager::get().create();

            if (!renderable) {
                qCritical() << "Could not create renderable entity";
            } else {
                RenderableManager::Builder builder(1);
                builder.boundingBox(rm.aabb)
                    .material(0, mat)
                    .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, rm.vb, rm.ib, 0, rm.indexCount)
                    .culling(false)
                    .castShadows(true)
                    .receiveShadows(true);
                auto result = builder.build(*mEngine, renderable);
                if (result != RenderableManager::Builder::Success) {
                    qCritical() << "Could not create renderable: " << node->mName.C_Str();
                    utils::EntityManager::get().destroy(renderable);
                } else {
                    mRenderables.push_back(renderable);
                    mScene->addEntity(renderable);

                    // Set the global transform for this node.
                    math::mat4f xform;
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            // note that aiMatrix is row-major and mat4f is col-major
                            xform[i][j] = transform[j][i];
                            //std::cout << transform[i][j] << ", ";
                        }
                        //std::cout << std::endl;
                    }
                    tcm.setTransform(tcm.getInstance(renderable), xform);
                    //tcm.setParent(tcm.getInstance(renderable), tcm.getInstance(mRoot));

                    qInfo() << "Created renderable: " << node->mName.C_Str();
                }
            }

        }
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        createRenderables(scene, node->mChildren[i], transform);
    }
}

//------------------------------------------------------------------------------

void FilamentRenderer::centerCamera()
{
    using namespace filament;
    auto &tcm = mEngine->getTransformManager();
    auto &rcm = mEngine->getRenderableManager();

    // Global bbox
    Box bbox;

    for (utils::Entity e : mRenderables) {
        math::mat4f xform = tcm.getWorldTransform(tcm.getInstance(e));
        Box aabb = rcm.getAxisAlignedBoundingBox(rcm.getInstance(e));

        Box xformed_box = rigidTransform(aabb, xform);

        if (bbox.isEmpty())
            bbox = xformed_box;
        else
            bbox.unionSelf(xformed_box);
    }

    math::float4 com_r = bbox.getBoundingSphere();

    double r = com_r[3];
    double s = std::tan(mFOV * (M_PI / 360));
    double zdist = (r / s);

    // set parent xform to move geo to origin
    math::mat4f xform(math::mat3f(), -com_r.xyz);
    if (!mCenterNode) {
        mCenterNode = utils::EntityManager::get().create();
        tcm.create(mCenterNode);
    }
    tcm.setTransform(tcm.getInstance(mCenterNode), xform);

    // set parent root xform
    for (utils::Entity e : mRenderables) {
        tcm.setParent(tcm.getInstance(e), tcm.getInstance(mCenterNode));
    }
    tcm.setParent(tcm.getInstance(mCenterNode), tcm.getInstance(mRoot));

    mZDist = zdist;
    mCamManipulator = CameraManipulator({ 0, 0, zdist},
                                        { 0, 0, 0 },
                                        { 0, 1, 0 });
    mCamManipulator.updateCamera(mMainCamera);
}

//------------------------------------------------------------------------------

void FilamentRenderer::cleanupTextures()
{
    for (auto mat_texs : mTextures) {
        if (mat_texs.albedo)
            mEngine->destroy(mat_texs.albedo);
        if (mat_texs.normalMap)
            mEngine->destroy(mat_texs.normalMap);
        if (mat_texs.aoMap)
            mEngine->destroy(mat_texs.aoMap);
        if (mat_texs.specMap)
            mEngine->destroy(mat_texs.specMap);
        if (mat_texs.maskMap)
            mEngine->destroy(mat_texs.maskMap);
    }
    mTextures.clear();
}

//------------------------------------------------------------------------------

void FilamentRenderer::cleanupMaterials()
{
    for (filament::MaterialInstance *mat : mMaterialInstances) {
        mEngine->destroy(mat);
    }
    mMaterialInstances.clear();
}

//------------------------------------------------------------------------------

void FilamentRenderer::cleanupRenderMeshes()
{
    for (RenderMesh &rm : mRenderMeshes) {
        mEngine->destroy(rm.vb);
        mEngine->destroy(rm.ib);
    }
    mRenderMeshes.clear();
}

//------------------------------------------------------------------------------

void FilamentRenderer::cleanupRenderables()
{
    for (utils::Entity e : mRenderables) {
        mScene->remove(e);

        mEngine->destroy(e);

        // destroy entities themselves
        utils::EntityManager::get().destroy(e);
    }
    mRenderables.clear();
}

//------------------------------------------------------------------------------

void FilamentRenderer::cleanupRenderElements()
{
    // Wait until all rendered operations are completed before we destroy
    // anything.
    filament::Fence::waitAndDestroy(mEngine->createFence());

    cleanupRenderables();
    cleanupRenderMeshes();
    cleanupMaterials();
    cleanupTextures();
}

//------------------------------------------------------------------------------

void FilamentRenderer::setScene(const aiScene *scene, std::string filename)
{
    // cleanup existing render elements
    cleanupRenderElements();

    QFileInfo fileinfo(QString(filename.c_str()));

    // Create material textures
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        std::string basedir = fileinfo.dir().canonicalPath().toStdString();
        aiMaterial *mat = scene->mMaterials[i];
        createMaterials(scene, mat, basedir);
    }

    // create meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        createRenderMesh(scene, scene->mMeshes[i]);
    }

    // create renderables from meshes
    if (!scene->mRootNode) {
        qCritical() << "No root found in scene";
        return;
    }
    createRenderables(scene, scene->mRootNode, aiMatrix4x4());

    centerCamera();
}
