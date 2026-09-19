#include "editor/EditorState.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    try{
        wf::EditorState state;
        require(state.entities().size()==1,"Initial cube missing");
        const auto first=state.selected()->id;const auto second=state.addCube();
        require(second!=first&&state.selection()==second,"Entity identity or selection failed");
        wf::Transform t;t.position={3,2,1};t.scale={2,3,4};
        require(state.setTransform(second,t),"Valid transform rejected");
        const auto center=t.matrix()*glm::vec4(0,0,0,1);
        require(glm::length(glm::vec3(center)-t.position)<.0001f,"Transform composition incorrect");
        auto bad=t;bad.scale.y=0;require(!state.setTransform(second,bad),"Singular scale accepted");
        bad=t;bad.position.x=std::numeric_limits<float>::quiet_NaN();require(!state.setTransform(second,bad),"NaN accepted");
        require(state.selected()->transform.position==t.position,"Rejected transform mutated state");
        state.camera.frame(*state.selected());require(state.camera.target==t.position,"Frame selection target wrong");
        state.camera.zoom(10000);require(state.camera.distance>=.2f,"Zoom underflow");state.camera.zoom(-10000);require(state.camera.distance<=200,"Zoom overflow");
        state.camera.orbit(0,100000);require(state.camera.pitch<=1.45f,"Orbit pole singularity");
        state.camera.pan(5,3);auto view=state.camera.view();for(int i=0;i<16;++i)require(std::isfinite(glm::value_ptr(view)[i]),"Nonfinite camera");
        require(!state.select(99999),"Invalid selection accepted");
        require(state.removeSelected()&&!state.selected(),"Delete did not clear selection");require(state.entities().size()==1,"Delete removed wrong count");
        state.newScene();require(state.selected()->id>second,"IDs reused after new scene");
        for(int i=0;i<600;++i)state.log("INFO","bounded");require(state.logs().size()==500,"Log buffer unbounded");
        std::cout<<"Editor state and camera checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
