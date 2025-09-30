//
// Created by pro on 2021/12/5.
//

#include "istool/basic/config.h"
#include "glog/logging.h"

using namespace std::literals;

#ifdef LINUX
const std::string config::KSourcePath = SOURCEPATH + "/"s;
#else
const std::string config::KSourcePath =  "/Users/pro/Desktop/work/2025S/SuFu-ML/";
#endif

#ifdef LINUX
const std::string config::KIncreParserPath = SOURCEPATH + "/surface/"s;
#else
// const std::string config::KIncreParserPath = "/Users/pro/Desktop/work/2025S/SuFu-ML/surface/"s;
const std::string config::KIncreParserPath = "/Users/pro/Desktop/work/2025S/SuFu-ML/interface/target/release/inp";
const std::string config::KIncrePrinterPath = "/Users/pro/Desktop/work/2025S/SuFu-ML/interface/target/release/oup";
#endif

const bool config::KIsDefaultSelf = true;

std::string global::KStageInfoPath = "";
TimeRecorder global::recorder;

void global::printStageResult(const std::string& info) {
    if (KStageInfoPath.empty()) {
        LOG(INFO) << info;
    } else {
        auto* f = std::fopen(KStageInfoPath.c_str(), "a");
        fprintf(f, "%s\n", info.c_str());
        fclose(f);
    }
}
