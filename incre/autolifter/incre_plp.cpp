//
// Created by pro on 2022/9/26.
//

#include "istool/incre/autolifter/incre_plp.h"
#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "glog/logging.h"
#include "istool/solver/enum/enum_util.h"
#include "istool/incre/trans/incre_trans.h"
#include <cassert>

using namespace incre::autolifter;
using namespace incre::grammar;
using namespace incre::example;
using namespace incre::analysis;


/*std::string FExampleSpace::example2String(const IOExample &example) {
    std::string res = "{";
    for (int i = 0; i < local_names.size(); ++i) {
        if (i) res += ", ";
        res += local_names[i] + ": " + example.first[i].toString();
    }
    res += "} -> " + example.second.toString();
    return res;
}*/
std::string FExampleSpace::example2String(int id) {
    return example_list[id]->toString();
}
void FExampleSpace::addExample() {
    int index = example_list.size();
    example_list.push_back(pool->example_pool[rewrite_id][index]);
}
int FExampleSpace::acquireExample(int target_num, TimeGuard *guard) {
    pool->generateBatchedExample(rewrite_id, target_num, guard);
    target_num = std::min(target_num, int(pool->example_pool[rewrite_id].size()));
    while (example_list.size() < target_num) {
        addExample();
    }
    return target_num;
}
FExampleSpace::FExampleSpace(IncreExamplePool *_pool, int _rewrite_id, const PEnv& _env, const RewriteTypeInfo& info):
        pool(_pool), rewrite_id(_rewrite_id), env(_env.get()) {
    for (auto& [var_name, var_type]: info.inp_types) {
        local_names.push_back(var_name);
        local_types.push_back(incre::trans::typeFromIncre(var_type.get()));
    }
    global_names = pool->global_name_list;
    for (auto& type: pool->global_type_list) {
        global_types.emplace_back(incre::trans::typeFromIncre(type.get()));
    }
}

void FExampleSpace::extendAuxCache(const AuxProgram &program, DataList *cache_item, int length) {
    assert(length <= example_list.size());
    for (int i = cache_item->size(); i < length; ++i) {
        cache_item->push_back(runAux(i, program));
    }
}
void FExampleSpace::extendOupCache(const PProgram &program, const std::vector<int> &path, DataList *cache_item,
                                   int length) {
    assert(length <= example_list.size());
    LOG(INFO) << "Extend from " << cache_item->size() << " to " << length;
    for (int i = cache_item->size(); i < length; ++i) {
        cache_item->push_back(runOup(i, program.get(), path));
    }
}

DataList *FExampleSpace::getAuxCache(const AuxProgram &program, int length) {
    auto feature = aux2String(program);
    if (aux_cache.find(feature) == aux_cache.end()) return nullptr;
    auto* cache_item = aux_cache[feature];
    extendAuxCache(program, cache_item, length);
    return cache_item;
}

namespace {
    std::string _path2String(const std::vector<int>& path) {
        std::string res = "[";
        for (int i = 0; i < path.size(); ++i) {
            if (i) res += ","; res += std::to_string(path[i]);
        }
        return res + "]";
    }

    std::string _getOupFeature(const PProgram& program, const std::vector<int>& path) {
        if (program) return program->toString() + "@" + _path2String(path);
        return _path2String(path);
    }
}

DataList* FExampleSpace::getOupCache(const PProgram &program, const std::vector<int> &path, int length) {
    auto feature = _getOupFeature(program, path);
    if (oup_cache.find(feature) == oup_cache.end()) return nullptr;
    auto* cache_item = oup_cache[feature];
    extendOupCache(program, path, cache_item, length);
    return cache_item;
}

void FExampleSpace::registerAuxCache(const AuxProgram &program, const DataList &oup_list) {
    auto feature = aux2String(program);
    assert(aux_cache.find(feature) == aux_cache.end());
    auto* cache_item = new DataList(oup_list);
    aux_cache[feature] = cache_item;
}
void FExampleSpace::registerOupCache(const PProgram &program, const std::vector<int> &path, const DataList& oup_list) {
    auto feature = _getOupFeature(program, path);
    assert(oup_cache.find(feature) == oup_cache.end());
    auto* cache_item = new DataList(oup_list);
    oup_cache[feature] = cache_item;
}

namespace {
    Data _extract(const Data& d, const std::vector<int>& path) {
        Data res(d);
        for (auto pos: path) {
            auto* v = dynamic_cast<incre::semantics::VTuple*>(res.get());
            assert(v && v->elements.size() > pos);
            res = v->elements[pos];
        }
        auto* cv = dynamic_cast<incre::semantics::VCompress*>(res.get());
        if (cv) return cv->body;
        return res;
    }
}

#include "istool/basic/config.h"

Data FExampleSpace::runExtract(int example_id, Program *prog) {
    global::recorder.start("execute");
    auto& example = example_list[example_id];
    auto res = env->run(prog, data::concatDataList(example->local_inputs, example->global_inputs));
    global::recorder.end("execute");
    return res;
}
Data FExampleSpace::runAux(int example_id, const Data& content, Program *prog) {
    global::recorder.start("execute");
    auto& example = example_list[example_id];
    auto res = env->run(prog, data::concatDataList({content}, example->global_inputs));
    global::recorder.end("execute");
    return res;
}

Data FExampleSpace::runAux(int example_id, const AuxProgram &aux) {
    auto compress = runExtract(example_id, aux.first.second.get());
    Data res;
    if (aux.second.second) {
        auto* tv = dynamic_cast<incre::semantics::VLabeledCompress*>(compress.get());
#ifdef DEBUG
        auto* mid_type = dynamic_cast<incre::trans::TLabeledCompress*>(aux.first.first.get());
        assert(tv && mid_type && tv->id == mid_type->id);
#endif
        return runAux(example_id, tv->body, aux.second.second.get());
    }
    return compress;
}
Data FExampleSpace::runOup(int example_id, Program *program, const std::vector<int>& path) {
    auto oup = _extract(example_list[example_id]->oup, path);
    if (program) {
        return runAux(example_id, oup, program);
    }
    return oup;
}

PLPTask::PLPTask(FExampleSpace *_example_space, const std::vector<GrammarEnumerateTool *> &_aux_grammar_list,
                 const std::vector<TypedProgramList> &_pre_res, GrammarEnumerateTool *_extract_grammar, const TypedProgram &_target,
                 const std::vector<int> &_path, int _oup_compress_id): example_space(_example_space), aux_grammar_list(_aux_grammar_list),
                 pre_res_list(_pre_res), extract_grammar(_extract_grammar), target(_target), path(_path), oup_compress_id(_oup_compress_id) {
    oup_cache = example_space->getOupCache(target.second, path, 0);
    if (!oup_cache) {
        example_space->registerOupCache(target.second, path, {});
        oup_cache = example_space->getOupCache(target.second, path, 0);
    }
}

void PLPTask::extendOupCache(int length) {
    example_space->extendOupCache(target.second, path, oup_cache, length);
}

// TODO: add cache
Data PLPTask::runOup(int example_id) {
    if (oup_cache->size() <= example_id) extendOupCache(example_id + 1);
    return oup_cache->at(example_id);
    //return example_space->runOup(example_id, target.second.get(), path);
}
Data PLPTask::runInp(int example_id, const AuxProgram& program) {
    return example_space->runAux(example_id, program);
}

IOExample PLPTask::getIO(int example_id, const std::vector<AuxProgram> &aux_list) {
    DataList inp;
    for (auto& aux_program: aux_list) {
        inp.push_back(runInp(example_id, aux_program));
    }
    return {inp, runOup(example_id)};
}

int PLPTask::acquireExample(int target_num, int timeout) {
    auto* guard = new TimeGuard(timeout);
    auto res = example_space->acquireExample(target_num, guard);
    delete guard;
    return res;
}

#define ValueHead(name) auto* v = dynamic_cast<incre::semantics::V ## name*>(data.get()); if (v)

Data incre::autolifter::eliminateCompress(const Data &data) {
    {ValueHead(Int) return data;}
    {ValueHead(Bool) return data;}
    {ValueHead(Compress) return v->body;}
    {
        ValueHead(Tuple) {
            DataList elements;
            for (int i = 0; i < v->elements.size(); ++i) {
                elements.push_back(eliminateCompress(v->elements[i]));
            }
            return Data(std::make_shared<incre::semantics::VTuple>(elements));
        }
    }
    {
        ValueHead(Ind) {
            return Data(std::make_shared<incre::semantics::VInd>(v->name, eliminateCompress(v->body)));
        }
    }
    LOG(FATAL) << "Unknown data " << data.toString();
}

Data incre::autolifter::openLabeledCompress(const Data &data, int label) {
    auto* v = dynamic_cast<incre::semantics::VLabeledCompress*>(data.get());
    if (!v || v->id != label) LOG(FATAL) << "Unmatched compress id: get " << data.toString() << " but except " << label;
    return eliminateCompress(data);
}

std::string incre::autolifter::aux2String(const AuxProgram &program) {
    if (!program.second.first) {
        return program.first.first->getName() + "@" + program.first.second->toString();
    } else {
        return /*program.first.first->getName() + "@" +*/ program.first.second->toString() + " -> " + program.second.second->toString();
    }
}
namespace {
    TypedProgram _extractTypedProgram(const PProgram& program) {
        auto* ts = dynamic_cast<incre::semantics::TypeLabeledDirectSemantics*>(program->semantics.get());
        assert(ts && ts->type);
        return {ts->type, program->sub_list[0]};
    }
}

void GrammarEnumerateTool::extend() {
    int target_size = program_pool.size();
    auto dummy_info = std::make_shared<SynthInfo>("", TypeList(), PType(), grammar);
    std::vector<FunctionContext> collect_list;
    auto* op = new RuleBasedOptimizer();
    EnumConfig c(nullptr, op, nullptr);
    solver::collectAccordingSize({dummy_info}, target_size + 1, collect_list, c);
    delete op;
    TypedProgramList res_list;
    for (auto& res: collect_list) {
        auto p = _extractTypedProgram(res[""]);
        if (p.second->size() == target_size) {
            res_list.push_back(p);
        }
    }
    program_pool.emplace_back(res_list);
}
TypedProgramList* GrammarEnumerateTool::acquirePrograms(int target_size) {
    // LOG(INFO) << "acquire program " << target_size << " " << size_limit;
    if (target_size > size_limit) return nullptr;
    while (target_size >= program_pool.size()) extend();
    return &program_pool[target_size];
}
GrammarEnumerateTool::~GrammarEnumerateTool() {
    delete grammar;
}