#include "gtjefficiencygraphic.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextCodec>

const QString c_strColorRanderStyle = "_crs";
const QString c_strGraphicStyle = "_gs";

CPUFreq::CPUFreq()
{
   QueryPerformanceFrequency(&freq);
}

CPUFreq::~CPUFreq()
{

}


static const CPUFreq s_freq;
static GTJProfileGlobalEnviroment s_enviroment;
static bool s_start = false;

GTJProfile::GTJProfile(std::wstring& name)
    :m_pName(std::move(name)), during(0.0), m_bStarted(false),
    m_nLeafHitCount(0), m_nChildHitCount(0), m_dLeafTotalDuring(0.0),
    m_dChildTotalDuring(0.0)
{
    if (s_start)
    {
        m_bStarted = true;
        s_enviroment.globalProfileStack.push(this);
        QueryPerformanceCounter(&start);
    }
}

GTJProfile::GTJProfile(const wchar_t* pName)
    :m_pName(pName),
    during(0.0), m_bStarted(false), m_nLeafHitCount(0),
    m_nChildHitCount(0), m_dLeafTotalDuring(0.0),
    m_dChildTotalDuring(0.0)
{
    if (s_start)
    {
        m_bStarted = true;
        s_enviroment.globalProfileStack.push(this);
        QueryPerformanceCounter(&start);
    }
}

GTJProfile::~GTJProfile()
{
    if (s_start || m_bStarted)
    {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        during = (double)(end.QuadPart - start.QuadPart) / (double)s_freq.freq.QuadPart;
        during *= 1000;
        s_enviroment.globalProfileStack.pop();
        if (!s_enviroment.globalProfileStack.empty())
        {
            //取出父级节点，告诉父节点自己析构了
            //1. 如果自己是叶子
            if(isLeaf()) 
            {
                m_dLeafTotalDuring = during;
                m_dChildTotalDuring = during;
                m_nLeafHitCount = 1;
                m_nChildHitCount = 1;
            }
            GTJProfile* parent = s_enviroment.globalProfileStack.top();
            parent->notifyDestruct(this);
            for (auto iter = m_oChildrenData.begin(); iter != m_oChildrenData.end(); ++iter)
            {
                delete iter->second;
            }
            m_oChildrenData.swap(GTJNestedProfileData());
        }
        else
        {
            //自己已经是最后一个了
            s_enviroment.profileDone(this);
        }
    }
}

void GTJProfile::notifyDestruct(GTJProfile* pProfile)
{
    //非叶子
    if (pProfile == nullptr)
    {
        return;
    }
    m_dLeafTotalDuring += pProfile->m_dLeafTotalDuring;
    m_dChildTotalDuring += pProfile->during;
    m_nChildHitCount += 1;
    m_nLeafHitCount += pProfile->m_nLeafHitCount;
    auto iter = m_oChildrenData.find(pProfile->m_pName);
    if (iter != m_oChildrenData.end())
    {
         GTJProfileSchema* pSchema = iter->second;
         pSchema->m_nHitCount += 1;
         if (pProfile->during > pSchema->m_dMax)
         {
             pSchema->m_dMax = pProfile->during;
         }
         pSchema->m_dTotal += pProfile->during;
         pSchema->m_dLeafTotalDuring += pProfile->m_dLeafTotalDuring;
         pSchema->m_dChildTotalDuring += pProfile->during;
         pSchema->m_nLeafHitCount += pProfile->m_nLeafHitCount;
         pSchema->m_nChildHitCount += pProfile->m_nChildHitCount;
         merge(pSchema->m_oChildren, pProfile->m_oChildrenData);
    }
    else
    {
        GTJProfileSchema* pNewData = new GTJProfileSchema(pProfile);
        m_oChildrenData[pNewData->m_pName] = pNewData;
    }
}

void GTJProfile::merge(GTJNestedProfileData& l, GTJNestedProfileData& r)
{
    for (auto riter = r.begin(); riter != r.end(); ++riter)
    {
        auto liter = l.find(riter->first);
        if (liter != l.end())
        {
            GTJProfileSchema* lData = liter->second;
            GTJProfileSchema* rData = riter->second;
            lData->m_nHitCount += rData->m_nHitCount;
            if (rData->m_dMax > lData->m_dMax)
            {
                lData->m_dMax = rData->m_dMax;
            }
            lData->m_dTotal += rData->m_dTotal;
            lData->m_dChildTotalDuring += rData->m_dChildTotalDuring;
            lData->m_dLeafTotalDuring += rData->m_dLeafTotalDuring;
            lData->m_nChildHitCount += rData->m_nChildHitCount;
            lData->m_nLeafHitCount += rData->m_nLeafHitCount;
            merge(lData->m_oChildren, rData->m_oChildren);
        }
        else
        {
            l[riter->first] = riter->second;
        }
    }
    r.clear();
}

bool GTJProfile::isLeaf()
{
    return m_oChildrenData.empty();
}

GTJProfileGlobalEnviroment::GTJProfileGlobalEnviroment()
    :m_nIndex(0), m_pHandle(nullptr), m_pWriter(nullptr)
{

}

GTJProfileGlobalEnviroment::~GTJProfileGlobalEnviroment()
{
    printSummary(true);
    freeJSONObjs();
    free();
}

void GTJProfileGlobalEnviroment::createFile()
{
   //创建一个json文件
   free();
   QString strDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmssz");
   m_pHandle = new QFile(QString("profile_%1.json").arg(strDateTime));
   m_pHandle->open(QIODevice::WriteOnly | QIODevice::Truncate);
   QTextCodec* pTextCode = QTextCodec::codecForName("UTF-8");
   m_pWriter = new QTextStream(m_pHandle);
   m_pWriter->setCodec(pTextCode);
}

void GTJProfileGlobalEnviroment::free()
{
    if (m_pHandle != nullptr)
    {
        m_pHandle->close();
        delete m_pHandle;
        m_pHandle = nullptr;
        if (m_pWriter != nullptr)
        {
            delete m_pWriter;
            m_pWriter = nullptr;
        }
    }
}

void GTJProfileGlobalEnviroment::profileDone(GTJProfile* pRootNode)
{
    GTJProfileSchema rootSchema(pRootNode);
    m_listJSONObjs.insert(m_listJSONObjs.end(), rootSchema.toJSON());
}

void GTJProfileGlobalEnviroment::freeJSONObjs()
{
    for (auto iter = m_listJSONObjs.begin(); iter != m_listJSONObjs.end(); ++iter)
    {
        delete *iter;
    }
    m_listJSONObjs.clear();
}

void GTJProfileGlobalEnviroment::printSummary(bool bClearCache)
{
    if (m_listJSONObjs.empty())
    {
        return;
    }
    QJsonObject oTotalJSONs;
    QJsonArray oRoots;
    for (auto iter = m_listJSONObjs.begin(); iter != m_listJSONObjs.end(); ++iter)
    {
        oRoots.append(**iter);
    }
    oTotalJSONs.insert("report", oRoots);
    printGlobalOptions(oTotalJSONs);
    printToFile(oTotalJSONs);
    if (bClearCache)
    {
        freeJSONObjs();
    }
}

void GTJProfileGlobalEnviroment::printToFile(QJsonObject& jsonObj)
{
    QJsonDocument oDocument(jsonObj);
    QString strJSON(oDocument.toJson(QJsonDocument::Compact));
    if (m_pHandle && m_pWriter)
    {
        *m_pWriter << strJSON;
        m_pHandle->flush();
    }
}

void GTJProfileGlobalEnviroment::setOptions(GTJEfficientGraphicOptions options)
{
    m_options = options;
}

void GTJProfileGlobalEnviroment::printGlobalOptions(QJsonObject& json)
{
    GTJEfficientGraphicOptions* pRoot = &m_options;
    QJsonObject optionJson;
    int nLevel = 0;
    QString strKeyTemplate = "%1";
    while(pRoot != nullptr)
    {
        QString strKey = strKeyTemplate.arg(nLevel++);
        optionJson.insert(strKey, pRoot->m_json);
        pRoot = pRoot->m_pChild;
    }
    json.insert("globalOption", optionJson);
}

void GTJProfileGlobalEnviroment::registerStandardValueCalcCallBack(int nLevel, GTJProfileStandardValueCalcCallBack& function)
{
    m_mapSVCCallBack[nLevel] = function;
}

void GTJProfileGlobalEnviroment::registerDifferenceValueCalcCallBack(int nLevel, GTJProfileDifferenceValueCalcCallBack& function)
{
    m_mapDVCCallBack[nLevel] = function;
}

bool wchat_tPtrLess::operator()(const std::wstring& l,const std::wstring& r)
{
    const wchar_t* pl = l.c_str();
    const wchar_t* pr = r.c_str();
    if (pl == nullptr) {return true;}
    else if(pr == nullptr) {return false;}
    else{
        return wcscmp(pl, pr) < 0;
    }
}

GTJProfileSchema::GTJProfileSchema()
    :m_pName(L"N/A"), m_nHitCount(0), m_dMax(0.0), m_dTotal(0.0), m_dChildTotalDuring(0.0), m_dLeafTotalDuring(0.0),
    m_nChildHitCount(0), m_nLeafHitCount(0)
{

}

GTJProfileSchema::GTJProfileSchema(GTJProfile* pProfile)
    :m_pName(L"N/A"), m_nHitCount(0), m_dMax(0.0), m_dTotal(0.0)
    , m_dChildTotalDuring(0.0), m_dLeafTotalDuring(0.0),
    m_nChildHitCount(0), m_nLeafHitCount(0)
{
    if (pProfile)
    {
        m_pName = pProfile->m_pName;
        m_nHitCount = 1;
        m_dMax = pProfile->during;
        m_dTotal = m_dMax;
        m_dChildTotalDuring = pProfile->m_dChildTotalDuring;
        m_dLeafTotalDuring = pProfile->m_dLeafTotalDuring;
        m_nChildHitCount = pProfile->m_nChildHitCount;
        m_nLeafHitCount = pProfile->m_nLeafHitCount;
        m_oChildren = std::move(pProfile->m_oChildrenData);
    }
}

GTJProfileSchema::~GTJProfileSchema()
{
    for (auto iter = m_oChildren.begin(); iter != m_oChildren.end() ; ++iter)
    {
        delete iter->second;
    }
    m_oChildren.swap(GTJNestedProfileData());
}

QString wchar_tPtrToQstring(std::wstring& p)
{
    QString res;
    if (!p.empty())
    {
        res = QString::fromStdWString(std::wstring(p));
    }
    else
    {
        res = "N/A";
    }
    return res;
}

__declspec(dllexport) void printSummary(bool clearCachedProfile)
{
    s_enviroment.printSummary(clearCachedProfile);
}

__declspec(dllexport) void startNewGraphic()
{
    s_enviroment.createFile();
}

__declspec(dllexport) void globalStart(bool bCreateFile)
{
    s_start = true;
    if (bCreateFile)
    {
        s_enviroment.createFile();
    }

}

__declspec(dllexport) void globalStop(bool bFreeFile)
{
    s_start = false;
    printSummary(true);
    if (bFreeFile)
    {
        s_enviroment.free();
    }
}

__declspec(dllexport) void setOptions(GTJEfficientGraphicOptions& options)
{
    s_enviroment.setOptions(options);
}

void registerStandardValueCalcCallBack(int nLevel, GTJProfileStandardValueCalcCallBack& function)
{
    s_enviroment.registerStandardValueCalcCallBack(nLevel, function);
}

void registerDifferenceValueCalcCallBack(int nLevel, GTJProfileDifferenceValueCalcCallBack& function)
{
    s_enviroment.registerDifferenceValueCalcCallBack(nLevel, function);
}

QJsonObject* GTJProfileSchema::toJSON(int nLevel)
{
    QJsonObject* pJsonObj = new QJsonObject();
    pJsonObj->insert("_pn" ,wchar_tPtrToQstring(this->m_pName));               //名字
    pJsonObj->insert("_ph", QString::number(this->m_nHitCount));           //命中次数
    pJsonObj->insert("_pm", QString::number(this->m_dMax, 'f', 3));             //单次最大耗时
    pJsonObj->insert("_pt", QString::number(this->m_dTotal, 'f', 3));     //自身总耗时
    double dStandardValue = this->calcStandardValue();
    auto standardCallBackIter = s_enviroment.m_mapSVCCallBack.find(nLevel);
    if (standardCallBackIter != s_enviroment.m_mapSVCCallBack.end())
    {
        dStandardValue = standardCallBackIter->second(this);
    }

    double dDifference = this->calcDifference(dStandardValue);
    auto diffCallBackIter = s_enviroment.m_mapDVCCallBack.find(nLevel);
    if(diffCallBackIter != s_enviroment.m_mapDVCCallBack.end())
    {
        dDifference = diffCallBackIter->second(this, dStandardValue);
    }
    bool bRoot = true;
    if (!m_oChildren.empty())
    {
        QJsonArray oChildrenArray;
        for (auto iter = m_oChildren.begin(); iter != m_oChildren.end(); ++iter)
        {
            GTJProfileSchema* pChild = iter->second;
            oChildrenArray.append(*pChild->toJSON(nLevel + 1));
            pJsonObj->insert("_pc", oChildrenArray);
        }
    }
    pJsonObj->insert("_sv", QString::number(dStandardValue, 'f', 4));
    pJsonObj->insert("_dv", QString::number(dDifference, 'f', 4));
    pJsonObj->insert("lv", nLevel);
    return pJsonObj;
}

double GTJProfileSchema::calcStandardValue()
{
    return m_dTotal / m_nHitCount;
}

double GTJProfileSchema::calcDifference(double dStandard)
{
    return m_dTotal - dStandard;
}

GTJEfficientGraphicOptions::GTJEfficientGraphicOptions(const GTJEfficientGraphicOptions& other)
    :m_pChild(nullptr)
{
    m_json = other.m_json;
    if (other.m_pChild != nullptr)
    {
        m_pChild = new GTJEfficientGraphicOptions(*other.m_pChild);
    }
}

GTJEfficientGraphicOptions::GTJEfficientGraphicOptions()
    :m_pChild(nullptr)
{
    m_json.insert(c_strColorRanderStyle, 0);
    m_json.insert(c_strGraphicStyle, 0);
}

void GTJEfficientGraphicOptions::setChildNodeOption(GTJEfficientGraphicOptions* pChildOption)
{
    m_pChild = new GTJEfficientGraphicOptions(*pChildOption);
}

void GTJEfficientGraphicOptions::setColorRenderStyle(GTJEfficientGraphicColorRenderStyle oStyle)
{
    m_json[c_strColorRanderStyle] = (int)(oStyle);
}

void GTJEfficientGraphicOptions::setGraphicStyle(GTJGraphicStyle oStyle)
{
    m_json[c_strGraphicStyle] = (int)(oStyle);
}

GTJEfficientGraphicOptions::~GTJEfficientGraphicOptions()
{
   if (m_pChild != nullptr)
   {
       delete m_pChild;
   }
}

GTJEfficientGraphicOptions& GTJEfficientGraphicOptions::operator=(const GTJEfficientGraphicOptions& other)
{
    m_json = other.m_json;
    if(other.m_pChild != nullptr)
    {
        m_pChild = new GTJEfficientGraphicOptions(*other.m_pChild);
    }
    return *this;
}
