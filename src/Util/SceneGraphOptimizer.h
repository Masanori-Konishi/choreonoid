#ifndef CNOID_UTIL_SCENE_GRAPH_OPTIMIZER_H
#define CNOID_UTIL_SCENE_GRAPH_OPTIMIZER_H

#include "exportdecl.h"

namespace cnoid {

class SgNode;
class SgGroup;
class CloneMap;

class CNOID_EXPORT SceneGraphOptimizer
{
public:
    SceneGraphOptimizer();
    ~SceneGraphOptimizer();

    SceneGraphOptimizer(const SceneGraphOptimizer&) = delete;
    SceneGraphOptimizer(SceneGraphOptimizer&&) = delete;
    SceneGraphOptimizer& operator=(const SceneGraphOptimizer&) = delete;
    SceneGraphOptimizer& operator=(SceneGraphOptimizer&&) = delete;

    //! \return Number of the simplified paths
    int simplifyTransformPathsWithTransformedMeshes(SgGroup* scene, CloneMap& cloneMap);
    
    //! \return Number of the removed element collections
    int removeUnusedMeshElements(SgNode* scene);

private:
    class Impl;
    Impl* impl;
};

}

#endif
