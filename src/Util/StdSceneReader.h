/**
   \file
   \author Shin'ichiro Nakaoka
*/

#ifndef CNOID_UTIL_STD_SCENE_READER_H
#define CNOID_UTIL_STD_SCENE_READER_H

#include "EigenUtil.h"
#include "SceneGraph.h"
#include "ValueTree.h"
#include <cnoid/stdx/filesystem>
#include "exportdecl.h"

namespace cnoid {

class YAMLReader;
class FilePathVariableProcessor;
  
class CNOID_EXPORT StdSceneReader
{
public:
    StdSceneReader();
    ~StdSceneReader();

    void setMessageSink(std::ostream& os);
    void setDefaultDivisionNumber(int n);
    int defaultDivisionNumber() const;

    // One of the settings is valid for the following two functions
    void setBaseDirectory(const std::string& directory);
    void setFilePathVariableProcessor(FilePathVariableProcessor* processor);

    std::string baseDirectory() const;
    stdx::filesystem::path baseDirPath() const;
    void setYAMLReader(YAMLReader* reader);
    void clear();
    void readHeader(Mapping* info);
    void readHeader(Mapping* info, double formatVersion);

    enum AngleUnit { DEGREE, RADIAN };
    void setAngleUnit(AngleUnit unit);
    bool isDegreeMode() const {
        return isDegreeMode_;
    }
    double toRadian(double angle) const {
        return isDegreeMode_ ? radian(angle) : angle;
    }
    float toRadian(float angle) const {
        return isDegreeMode_ ? radian(angle) : angle;
    }
    bool readAngle(const Mapping& info, const char* key, double& angle) const;
    bool readAngle(const Mapping& info, const char* key, float& angle) const;
    bool readRotation(const Mapping& info, Matrix3& out_R) const;
    bool readRotation(const Mapping& info, const char* key, Matrix3& out_R) const;
    bool extractRotation(Mapping& info, Matrix3& out_R) const;
    bool readTranslation(const Mapping& info, Vector3& out_p) const;
    bool readTranslation(const Mapping& info, const char* key, Vector3& out_p) const;
    bool extractTranslation(Mapping& info, Vector3& out_p) const;
    SgNode* readNode(Mapping& info);
    SgNode* readNode(Mapping& info, const std::string& type);
    SgNode* readScene(ValueNode* scene);

    struct Resource {
        SgNodePtr scene;
        ValueNodePtr info;
        std::string uri;
        std::string directory;
    };
    Resource readResourceNode(Mapping& info);
    
    SgObject* readObject(Mapping& info);

    typedef std::function<std::string(const std::string& path, std::ostream& os)> UriSchemeHandler;
    
    static void registerUriSchemeHandler(const std::string& scheme, UriSchemeHandler handler);

private:
    class Impl;
    Impl* impl;

    bool isDegreeMode_;
    AngleAxis readAngleAxis(const Listing& rotation) const;
    bool readRotation(const ValueNode* info, Matrix3& out_R) const;
    bool readTranslation(const ValueNode* info, Vector3& out_p) const;
};

}

#endif
