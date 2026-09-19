#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wf {
using EntityId = std::uint64_t;
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 matrix() const;
};
struct Entity {
    EntityId id;
    std::string name;
    Transform transform;
    glm::vec3 color{0.77f, 0.57f, 0.29f};
    bool visible{true};
};
struct OrbitCamera {
    glm::vec3 target{0.0f, 0.5f, 0.0f};
    float yaw{0.7f}, pitch{0.42f}, distance{7.0f};
    glm::vec3 eye() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float amount);
    void frame(const Entity& entity);
};
struct LogEntry { std::string level; std::string message; };
class EditorState {
public:
    EditorState();
    EntityId addCube();
    bool removeSelected();
    bool select(EntityId id);
    Entity* selected();
    const Entity* selected() const;
    bool setTransform(EntityId id, const Transform& value);
    void newScene();
    void log(std::string level, std::string message);
    const std::vector<Entity>& entities() const { return entities_; }
    const std::vector<LogEntry>& logs() const { return logs_; }
    std::optional<EntityId> selection() const { return selection_; }
    OrbitCamera camera;
    bool grid{true}, wireframe{false};
private:
    std::vector<Entity> entities_;
    std::vector<LogEntry> logs_;
    std::optional<EntityId> selection_;
    EntityId nextId_{1};
};
}
