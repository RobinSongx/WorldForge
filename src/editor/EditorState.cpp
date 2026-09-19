#include "editor/EditorState.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace wf {
glm::mat4 Transform::matrix() const {
    auto m = glm::translate(glm::mat4(1), position);
    m = glm::rotate(m, glm::radians(rotationDegrees.z), {0,0,1});
    m = glm::rotate(m, glm::radians(rotationDegrees.y), {0,1,0});
    m = glm::rotate(m, glm::radians(rotationDegrees.x), {1,0,0});
    return glm::scale(m, scale);
}
glm::vec3 OrbitCamera::eye() const {
    return target + distance * glm::vec3(std::cos(pitch)*std::sin(yaw), std::sin(pitch), std::cos(pitch)*std::cos(yaw));
}
glm::mat4 OrbitCamera::view() const { return glm::lookAt(eye(), target, glm::vec3(0,1,0)); }
glm::mat4 OrbitCamera::projection(float aspect) const {
    return glm::perspective(glm::radians(45.0f), std::max(aspect, 0.01f), 0.05f, 500.0f);
}
void OrbitCamera::orbit(float dx, float dy) { yaw -= dx * 0.008f; pitch = std::clamp(pitch + dy * 0.008f, -1.45f, 1.45f); }
void OrbitCamera::pan(float dx, float dy) {
    const auto forward = glm::normalize(target - eye());
    const auto right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
    const auto up = glm::cross(right, forward);
    target += (-dx * right + dy * up) * distance * 0.0015f;
}
void OrbitCamera::zoom(float amount) { distance = std::clamp(distance * std::exp(-amount*0.15f), 0.2f, 200.0f); }
void OrbitCamera::frame(const Entity& entity) {
    target = entity.transform.position;
    distance = std::clamp(glm::length(entity.transform.scale)*2.0f, 1.5f, 200.0f);
}
EditorState::EditorState() { newScene(); }
EntityId EditorState::addCube() {
    Entity e{nextId_++, "Cube"};
    e.name += " " + std::to_string(e.id);
    e.transform.position = {static_cast<float>(entities_.size())*1.4f, 0.5f, 0.0f};
    selection_ = e.id;
    entities_.push_back(e);
    log("INFO", "Created " + e.name);
    return e.id;
}
bool EditorState::removeSelected() {
    if (!selection_) return false;
    const auto id = *selection_;
    auto it = std::find_if(entities_.begin(), entities_.end(), [id](const Entity& e){ return e.id==id; });
    if (it==entities_.end()) return false;
    log("INFO", "Deleted " + it->name);
    entities_.erase(it); selection_.reset(); return true;
}
bool EditorState::select(EntityId id) {
    if (std::none_of(entities_.begin(),entities_.end(),[id](const Entity& e){return e.id==id;})) return false;
    selection_=id; return true;
}
Entity* EditorState::selected() {
    if (!selection_) return nullptr;
    auto it=std::find_if(entities_.begin(),entities_.end(),[this](const Entity& e){return e.id==*selection_;});
    return it==entities_.end()?nullptr:&*it;
}
const Entity* EditorState::selected() const { return const_cast<EditorState*>(this)->selected(); }
bool EditorState::setTransform(EntityId id, const Transform& value) {
    for (int i=0;i<3;++i) {
        if (!std::isfinite(value.position[i]) || !std::isfinite(value.rotationDegrees[i]) ||
            !std::isfinite(value.scale[i]) || value.scale[i]<0.01f || value.scale[i]>100.0f) return false;
    }
    for (auto& e:entities_) if(e.id==id){ e.transform=value; return true; }
    return false;
}
void EditorState::newScene() { entities_.clear(); selection_.reset(); camera=OrbitCamera{}; addCube(); log("INFO", "New procedural scene. File import arrives in the Mesh milestone."); }
void EditorState::log(std::string level,std::string message) {
    if(logs_.size()>=500) logs_.erase(logs_.begin());
    logs_.push_back({std::move(level),std::move(message)});
}
}
