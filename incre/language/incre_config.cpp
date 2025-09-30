//
// Created by pro on 2023/12/6.
//
#include "istool/incre/language/incre_program.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::config;

const std::string config::KDataSizeLimitName = "incre@data-size-limit";
const std::string config::KIsNonLinearName = "incre@is-non-linear";
const std::string config::KIsEnableFoldName = "incre@is-enable-fold";
const std::string config::KSampleIntMaxName = "incre@sample-int-max";
const std::string config::KSampleIntMinName = "incre@sample-int-min";
const std::string config::KPrintAlignName = "incre@print-align";
const std::string config::KComposeNumName = "AutoLifter@ComposedNum";
const std::string config::KVerifyBaseName = "OccamVerifier@ExampleNum";
const std::string config::KMaxTermNumName = "PolyGen@MaxTermNum";
const std::string config::KMaxClauseNumName = "DNF@MaxClauseName";
const std::string config::KThreadNumName = "incre@thread-num";
const std::string config::KSlowCombineName = "incre@slow-combine";

namespace {
    const int KDefaultDataSizeLimit = 10;
    const bool KDefaultIsNonLinear = false;
    const bool KDefaultIsEnableFold = true;
    const int KDefaultSampleIntMax = 5;
    const int KDefaultSampleIntMin = -5;
    const bool KDefaultIsPrintAlign = false;
    const int KDefaultComposeNum = 3;
    const int KDefaultVerifyBase = 1000;
    const int KDefaultMaxTermNum = 4;
    const int KDefaultMaxClauseNum = 5;
    const int KDefaultThreadNum = 4;
    const bool KDefaultSlowCombine = false;

}

#include "istool/sygus/theory/basic/clia/clia.h"

IncreConfigMap config::buildDefaultConfigMap() {
    return {
            {IncreConfig::SAMPLE_SIZE, BuildData(Int, KDefaultDataSizeLimit)},
            {IncreConfig::NON_LINEAR, BuildData(Bool, KDefaultIsNonLinear)},
            {IncreConfig::ENABLE_FOLD, BuildData(Bool, KDefaultIsEnableFold)},
            {IncreConfig::SAMPLE_INT_MIN, BuildData(Int, KDefaultSampleIntMin)},
            {IncreConfig::SAMPLE_INT_MAX, BuildData(Int, KDefaultSampleIntMax)},
            {IncreConfig::PRINT_ALIGN, BuildData(Bool, KDefaultIsPrintAlign)},
            {IncreConfig::COMPOSE_NUM, BuildData(Int, KDefaultComposeNum)},
            {IncreConfig::VERIFY_BASE, BuildData(Int, KDefaultVerifyBase)},
            {IncreConfig::TERM_NUM, BuildData(Int, KDefaultMaxTermNum)},
            {IncreConfig::CLAUSE_NUM, BuildData(Int, KDefaultMaxClauseNum)},
            {IncreConfig::THREAD_NUM, BuildData(Int, KDefaultThreadNum)},
            {IncreConfig::SLOW_COMBINE, BuildData(Bool, KDefaultSlowCombine)}
    };
}

namespace {
    std::string _getConfigName(IncreConfig config) {
        switch (config) {
            case IncreConfig::SAMPLE_SIZE: return config::KDataSizeLimitName;
            case IncreConfig::NON_LINEAR: return config::KIsNonLinearName;
            case IncreConfig::ENABLE_FOLD: return config::KIsEnableFoldName;
            case IncreConfig::SAMPLE_INT_MIN: return config::KSampleIntMinName;
            case IncreConfig::SAMPLE_INT_MAX: return config::KSampleIntMaxName;
            case IncreConfig::PRINT_ALIGN: return config::KPrintAlignName;
            case IncreConfig::COMPOSE_NUM: return config::KComposeNumName;
            case IncreConfig::VERIFY_BASE: return config::KVerifyBaseName;
            case IncreConfig::TERM_NUM: return config::KMaxTermNumName;
            case IncreConfig::CLAUSE_NUM: return config::KMaxClauseNumName;
            case IncreConfig::THREAD_NUM: return config::KThreadNumName;
            case IncreConfig::SLOW_COMBINE: return config::KSlowCombineName;
        }
    }
}

void config::applyConfig(IncreProgramData *program, Env *env) {
    auto default_map = buildDefaultConfigMap();
    for (auto& [name, val]: default_map) {
        auto it = program->config_map.find(name);
        auto config_name = _getConfigName(name);
        if (it == program->config_map.end()) {
            env->setConst(config_name, val);
        } else env->setConst(config_name, it->second);
    }
}
