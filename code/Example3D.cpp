/*TODO
    Adding materials to Model: the way we are currently handling this
    is quite... meh. Dont pre-size the Model._submeshMaterials
    it's too error prone.
    Instead do Model._submeshMaterials.push_back()

    [Suggestions]:
        - global asset hash maps ?
            each time a model is created, the global Meshes hash table will be checked
            if no entry for that model, it'll create one


    ---------------------------ANIMATION STUFF-------------------------
    - how to find the root joint? :
        the root joint is the last element in the data->nodes[] array

    - each joint has a matrix called inverseBindMatrix
            transforms vertices into the local space of the joint

    - "each joint node may have a local tranform and an array of children"
    - "the bones of the skeleton are given implicitly, as the connections between the joints"

    - JOINTS_0 & WEIGHTS_0 refer to an accessor
        JOINTS_0 contains the indices of the joints affecting the vertex: 8bit vec4(j0, j1, j2, j3)
        WIGHTS_0 defines the weights: vec4(w0, w1, w2, w3)
        from these informations the "Skinning Matrix" can be computed
            see "Computing the skinning matrix"

    - reading:
        - https://gamemath.com/book/multiplespaces.html
        - https://computergraphics.stackexchange.com/questions/7603/confusion-about-how-inverse-bind-pose-is-actually-calculated-and-used#:~:text=The%20inverse%20bind%20pose%2C%20which,*%20v%2C%20which%20makes%20sense.
        - https://www.khronos.org/opengl/wiki/Skeletal_Animation
        - https://www.freecodecamp.org/news/advanced-opengl-animation-technique-skeletal-animations/
        - https://moddb.fandom.com/wiki/OpenGL:Tutorials:Basic_Bones_System#How_does_a_bone_system_work?
        // https://www.gamedev.net/forums/topic/706777-optimizing-skeletal-animation-system/
*/


// https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_004_ScenesNodes.md
// https://kcoley.github.io/glTF/specification/2.0/figures/gltfOverview-2.0.0a.png
// https://www.khronos.org/files/gltf20-reference-guide.pdf
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "adgl.h"

#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Mesh.h"
#include "Animation.h"
#include "Point.h"

// todo: should these be provided as globals?
// how should we deal with general window parameters?
extern uint32_t WND_WIDTH;
extern uint32_t WND_HEIGHT;

static Camera camera = {};

/** note:
 * we might want to find a proper way to handle
 * assets down the road if this becomes a problem
 */

struct Ground
{
    Material *material;
    uint32_t  VAO;
} ground;

struct Entity
{
    Model2 model;
};

std::vector<Entity> entities {};

void Example3DInit()
{
    camera.CameraCreate({ 0, 5, 16 }, 45.f, WND_WIDTH / (float)WND_HEIGHT, .8f, 1000.f);
    gCameraInUse = &camera;

    gTextures.insert({ "brick_wall", TextureCreate("assets/brick_wall.jpg") });
    gTextures.insert({ "cs_blend_red Base Color", TextureCreate("assets/cs_blend_red Base Color.png") });
    gTextures.insert({ "ground", TextureCreate("assets/ground.jpg") });
    gTextures.insert({ "cube", TextureCreate("assets/cube.png") });
    gTextures.insert({ "Material-1", TextureCreate("assets/Material-1.png") });
    gTextures.insert({ "Material-2", TextureCreate("assets/Material-2.png") });
    gTextures.insert({ "Material", TextureCreate("assets/Material.png") });


    gMaterials.insert({ "standard", MaterialCreate("shaders/point.shader", NULL) });
    gMaterials.insert({ "mileMaterial", MaterialCreate("shaders/standard.shader", gTextures["brick_wall"]) });
    gMaterials.insert({ "cubeMaterial", MaterialCreate("shaders/standard.shader", gTextures["cube"]) });
    gMaterials.insert({ "chibiMaterial", MaterialCreate("shaders/standard.shader", gTextures["cs_blend_red Base Color"]) });
    gMaterials.insert({ "groundMaterial", MaterialCreate("shaders/ground.shader", gTextures["ground"]) });



    // PrintAnimationClipTransforms();


    std::string lowPoly01       = "low_poly_01.gltf";
    std::string char05Milestone = "char_05_milestone.gltf";

    std::string path = "assets/" + char05Milestone;


    entities.resize(3);

    entities[0].model.Create(path.c_str());
    entities[0].model._meshes[0]._materials[0] = gMaterials["mileMaterial"];
    entities[0].model._meshes[0]._materials[1] = gMaterials["mileMaterial"];
    entities[0].model._meshes[0]._materials[2] = gMaterials["mileMaterial"];
    entities[0].model._transform.translation   = { 0.f, 0.f, -6.f };
    entities[0].model._transform.rotation      = { 0.f, 0.f, 0.f };
    entities[0].model._transform.scale         = { .05, .05, .05 };


    entities[1].model.Create("assets/skinned_cube.gltf");
    entities[1].model._meshes[0]._materials[0] = gMaterials["cubeMaterial"];
    entities[1].model._transform.translation   = { -4.f, 0, -6.f };
    entities[1].model._transform.rotation      = { 0, 0, 0 };
    entities[1].model._transform.scale         = { 1, 1, 1 };


    entities[2].model.Create("assets/chibi_02_ex.gltf");
    entities[2].model._meshes[0]._materials[0] = gMaterials["chibiMaterial"];
    entities[2].model._transform.translation   = { 4.f, 0.f, -6.f };
    entities[2].model._transform.rotation      = { 0.f, 0.f, 0.f };
    entities[2].model._transform.scale         = { 0.05f, 0.05f, 0.05f };


    ground.material = gMaterials["groundMaterial"];
    glGenVertexArrays(1, &ground.VAO);
}


void Example3DUpdateDraw(float dt, Input input)
{
    gCameraInUse->_aspect = WND_WIDTH / (float)WND_HEIGHT;
    gCameraInUse->CameraUpdate(input, dt);

    // draw wireframe
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);



    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //
    // Ground plane
    //
    ShaderUse(ground.material->_shader->programID);
    ShaderSetMat4ByName("proj", gCameraInUse->_projection, ground.material->_shader->programID);
    ShaderSetMat4ByName("view", gCameraInUse->_view, ground.material->_shader->programID);
    glBindTexture(GL_TEXTURE_2D, ground.material->_texture->id);
    glBindVertexArray(ground.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    //
    // Entities
    //
    for (size_t n = 0; n < entities.size(); n++) {
        for (int i = 0; i < entities[n].model._meshes.size(); i++) {
            glm::mat4 model = glm::mat4(1);

            model = glm::translate(model, entities[n].model._transform.translation)
                * glm::rotate(model, (Radians(entities[n].model._transform.rotation.x)), glm::vec3(1, 0, 0))
                * glm::rotate(model, (Radians(entities[n].model._transform.rotation.y)), glm::vec3(0, 1, 0))
                * glm::rotate(model, (Radians(entities[n].model._transform.rotation.z)), glm::vec3(0, 0, 1))
                * glm::scale(model, entities[n].model._transform.scale);

            for (int j = 0; j < entities[n].model._meshes[i]._VAOs.size(); j++) {

                Material *material = entities[n].model._meshes[i]._materials[j];
                ShaderUse(material->_shader->programID);
                ShaderSetMat4ByName("projection", gCameraInUse->_projection, material->_shader->programID);
                ShaderSetMat4ByName("view", gCameraInUse->_view, material->_shader->programID);
                ShaderSetMat4ByName("model", model, material->_shader->programID);
                glBindTexture(GL_TEXTURE_2D, material->_texture->id);

                Mesh2 mesh = entities[n].model._meshes[i];
                glBindVertexArray(mesh._VAOs[j]);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entities[n].model._DataBufferID);
                glDrawElements(GL_TRIANGLES, mesh._indicesCount[j], GL_UNSIGNED_SHORT, (void *)mesh._indicesOffsets[j]);
            }
        }
    }
}