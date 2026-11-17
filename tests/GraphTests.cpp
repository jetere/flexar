#include "Graph.h"
#include <iostream>
#include <stdexcept>
using namespace flexar;
void check(bool value) { if(!value)throw std::runtime_error("Graph test failed"); }
int main() {
    Graph g;std::string error;
    auto s=g.add(Effect::Source), a=g.add(Effect::Grade), b=g.add(Effect::Grain), o=g.add(Effect::Output);
    auto in=[&](Id n){return g.find(n)->inputs[0].id;};auto out=[&](Id n){return g.find(n)->outputs[0].id;};
    check(g.connect(out(s),in(a),error)); check(g.connect(out(a),in(b),error));check(g.connect(out(b),in(o),error));
    check(!g.connect(out(b),in(a),error));check(g.connections.size()==3);check(g.connections[0].from==out(s));
    check(!g.connect(in(a),out(b),error));check(!g.connect(9999,in(a),error));
    auto unused=g.add(Effect::Blur);check(g.execution(o).size()==4);check(g.sorted().size()==5);
    check(g.connect(out(s),in(b),error));check(g.execution(o).size()==3);
    g.remove(b);check(g.connections.size()==1);g.remove(unused);
    for(int i=0;i<PresetCount;++i){g.preset(i);check(g.execution(g.nodes.back().id).size()==g.nodes.size());}
    Graph branches;auto source=branches.add(Effect::Source),left=branches.add(Effect::Grade),right=branches.add(Effect::Grain),blend=branches.add(Effect::Blend),output=branches.add(Effect::Output);
    auto wire=[&](Id from,Id to,size_t input=0){return branches.connect(branches.find(from)->outputs[0].id,branches.find(to)->inputs[input].id,error);};
    check(branches.find(blend)->inputs.size()==2);check(wire(source,left));check(wire(source,right));check(wire(left,blend));check(wire(right,blend,1));check(wire(blend,output));
    check(branches.execution(output).size()==5);check(!wire(blend,left));check(branches.connections.size()==5);
    branches.disconnect(branches.find(blend)->inputs[1].id);check(branches.execution(output).size()==4);
    for(auto& effect:catalog()){auto id=branches.add(effect.effect);check(!branches.find(id)->parameters.empty());}
    std::cout<<"Graph tests passed: cycle rollback, pin validation, replacement, pruning, deletion, presets\n";
}
