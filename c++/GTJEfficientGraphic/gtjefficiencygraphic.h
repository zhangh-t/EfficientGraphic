#ifndef GTJEFFICIENCYGRAPHIC_H
#define GTJEFFICIENCYGRAPHIC_H

#include <Windows.h>
#include "Common/scopeguard.h"
#include <hash_map>
#include <QFile>
#include <QTextStream>
#include <stack>
#include <QJsonObject>
#include <QFlag>
#include <functional>

typedef __int64 int64_t;
class CPUFreq
{
public:
    CPUFreq();
    ~CPUFreq();
public:
    LARGE_INTEGER freq;
};

class GTJProfileSchema;
class GTJProfile;
enum GTJEfficientGraphicColorRenderStyle{
    crsDefault = 0,
    crsByDiff = 1,
};

enum GTJGraphicStyle{
    scrPie = 0,
    srcScatter,
    srcRader,
};

class GTJProfileGlobalEnviroment;
class __declspec(dllexport) GTJEfficientGraphicOptions
{
    friend class GTJProfileSchema;
    friend class GTJEfficientGraphicOptions;
    friend class GTJProfileGlobalEnviroment;
public:
    GTJEfficientGraphicOptions();
    GTJEfficientGraphicOptions(const GTJEfficientGraphicOptions& other);    //浅拷贝
    GTJEfficientGraphicOptions& operator = (const GTJEfficientGraphicOptions& other);   //浅拷贝
    virtual ~GTJEfficientGraphicOptions();
public:
    void setChildNodeOption(GTJEfficientGraphicOptions* pChildOption);      //里头不管内存
    void setColorRenderStyle(GTJEfficientGraphicColorRenderStyle oStyle);
    void setGraphicStyle(GTJGraphicStyle oStyle);
private:
    QJsonObject m_json;
    GTJEfficientGraphicOptions* m_pChild;
};

typedef std::function<double (GTJProfileSchema*)> GTJProfileStandardValueCalcCallBack;
typedef std::function<double (GTJProfileSchema*, double)> GTJProfileDifferenceValueCalcCallBack;

class GTJProfileGlobalEnviroment
{
    friend class GTJProfile;
    friend class GTJProfileSchema;
private:
    std::stack<GTJProfile*> globalProfileStack;
public:
    void registerStandardValueCalcCallBack(int nLevel, GTJProfileStandardValueCalcCallBack& function);
    void registerDifferenceValueCalcCallBack(int nLevel, GTJProfileDifferenceValueCalcCallBack& function);
public:
    GTJProfileGlobalEnviroment();
    ~GTJProfileGlobalEnviroment();
public:
    void setOptions(GTJEfficientGraphicOptions options);
    void printSummary(bool bClearCache);
    void createFile();
    void free();
private:
    void profileDone(GTJProfile* pRootNode);
    void printToFile(QJsonObject& jsonObj);
    void freeJSONObjs();
    void printGlobalOptions(QJsonObject& json);
private:
    int m_nIndex;
private:
    QFile* m_pHandle;
    QTextStream* m_pWriter;
    std::list<QJsonObject*> m_listJSONObjs;
    GTJEfficientGraphicOptions m_options;
private:
    std::map<int, GTJProfileStandardValueCalcCallBack> m_mapSVCCallBack;
    std::map<int, GTJProfileDifferenceValueCalcCallBack> m_mapDVCCallBack;
};

struct wchat_tPtrLess
{
    bool operator () (const std::wstring& l,const std::wstring& r);
};

typedef std::map<std::wstring, GTJProfileSchema*, wchat_tPtrLess> GTJNestedProfileData;


class __declspec(dllexport) GTJProfileSchema
{
    friend class GTJProfileGlobalEnviroment;
public:
    GTJProfileSchema();
    GTJProfileSchema(GTJProfile* pProfile);
    virtual ~GTJProfileSchema();
private:
    QJsonObject* toJSON(int nLevel = 0);  //生成json
    double calcStandardValue();  //默认的基准值计算方法   散点图要根据这个值绘制基准线
    double calcDifference(double dStandard);    //默认的差值计算方法     饼图要根据这个值设置颜色
public:
    std::wstring m_pName;
    int64_t m_nHitCount;
    double m_dMax;
    double m_dTotal;        //结束-开始的时间
    int64_t m_nChildHitCount;   //子节点的命中次数
    int64_t m_nLeafHitCount;    //叶子节点上的命中次数
    double m_dLeafTotalDuring;   //叶子上的总耗时
    double m_dChildTotalDuring;  //子节点的总耗时
    GTJNestedProfileData m_oChildren;
};



class __declspec(dllexport) GTJProfile
{
    friend class GTJProfile;
    friend class GTJProfileSchema;
public:
    GTJProfile(std::wstring& name);
    GTJProfile(const wchar_t* pName);
    ~GTJProfile();
public:
    bool isLeaf();
private:
    void notifyDestruct(GTJProfile* pProfile);
private:
    void merge(GTJNestedProfileData& l, GTJNestedProfileData& r);
private:
    std::wstring m_pName;
    LARGE_INTEGER start;
    double during;
    GTJNestedProfileData m_oChildrenData;
    int64_t m_nLeafHitCount;    //叶子节点的命中次数
    int64_t m_nChildHitCount;    //子节点的命中次数
    double m_dLeafTotalDuring;   //叶子节点上的总耗时
    double m_dChildTotalDuring;  //子节点上的总耗时
    bool m_bStarted;
private:
    int m_nLevel;
};

#define PASTE_IMP(A, B) A##B
#define PASTE(A, B) PASTE_IMP(A, B)

#ifdef __COUNTER__
#define GTJ_PROFILE(name) \
    GTJProfile PASTE(item, __COUNTER__)(name);
#else
#define GTJ_PROFILE(name) \
    GTJProfile PASTE(item, __LINE__)(name);
#endif

__declspec(dllexport) void registerStandardValueCalcCallBack(int nLevel, GTJProfileStandardValueCalcCallBack& function);
__declspec(dllexport) void registerDifferenceValueCalcCallBack(int nLevel, GTJProfileDifferenceValueCalcCallBack& function);
__declspec(dllexport) void setOptions(GTJEfficientGraphicOptions& options);
__declspec(dllexport) void globalStart(bool bCreateFile);
__declspec(dllexport) void globalStop(bool bFreeFile);
__declspec(dllexport) void printSummary(bool clearCachedProfiles = true);
__declspec(dllexport) void startNewGraphic();

#endif // GTJEFFICIENCYGRAPHIC_H
