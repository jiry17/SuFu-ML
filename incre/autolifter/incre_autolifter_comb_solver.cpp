//
// Created by pro on 2022/11/18.
//

#include <istool/basic/bitset.h>
#include <istool/ext/deepcoder/deepcoder_semantics.h>
#include "istool/incre/autolifter/incre_autolifter_solver.h"
#include "istool/solver/autolifter/basic/streamed_example_space.h"
#include "istool/incre/trans/incre_trans.h"
#include "istool/incre/autolifter/incre_solver_util.h"
#include "istool/invoker/invoker.h"
#include "istool/solver/stun/stun.h"
#include "glog/logging.h"
#include <iostream>

using namespace incre;
using namespace incre::autolifter;
using namespace incre::semantics;
using namespace incre::syntax;
using namespace incre::example;

namespace {
    struct _OutputCase {
        std::vector<int> path;
        TypedProgram program;
        Env* env;
        _OutputCase(const std::vector<int>& _path, const TypedProgram& _program, Env* _env): path(_path), program(_program), env(_env) {}
        Data extract(const Data& d, const DataList& global_inputs) const {
            auto res = d;
            for (auto pos: path) {
                auto* cv = dynamic_cast<VCompress*>(res.get());
                if (cv) {
                    try {
                        return env->run(program.second.get(), data::concatDataList({cv->body}, global_inputs));
                    } catch (SemanticsError& e) {
                        return {};
                    }
                }
                auto* tv = dynamic_cast<VTuple*>(res.get());
                assert(tv);
                res = tv->elements[pos];
            }
            return res;
        }
    };

    void _collectOutputCase(const Ty& type, std::vector<int>& path, std::vector<_OutputCase>& res, const std::vector<FRes>& f_res_list, Env* env) {
        if (type->getType() == TypeType::TUPLE) {
            auto* tt = dynamic_cast<TyTuple*>(type.get());
            for (int i = 0; i < tt->fields.size(); ++i) {
                path.push_back(i);
                _collectOutputCase(tt->fields[i], path, res, f_res_list, env);
                path.pop_back();
            }
        } else if (type->getType() == TypeType::COMPRESS) {
            auto* ct = dynamic_cast<TyLabeledCompress*>(type.get());
            assert(ct);
            if (f_res_list[ct->id].component_list.size() == 1) {
                res.emplace_back(path, f_res_list[ct->id].component_list[0].program,env);
            } else {
                for (int i = 0; i < f_res_list[ct->id].component_list.size(); ++i) {
                    path.push_back(i);
                    auto &oup_component = f_res_list[ct->id].component_list[i];
                    res.emplace_back(path, oup_component.program, env);
                    path.pop_back();
                }
            }
        } else res.emplace_back(path, std::pair<PType, PProgram>(incre::trans::typeFromIncre(type.get()), nullptr), env);
    }

    std::vector<_OutputCase> _collectOutputCase(int rewrite_id, IncreAutoLifterSolver* solver) {
        auto oup_type = solver->info->rewrite_info_list[rewrite_id].oup_type;
        std::vector<int> path; std::vector<_OutputCase> res;
        _collectOutputCase(oup_type, path, res, solver->f_res_list, solver->env.get());
        return res;
    }

    class CExampleSpace {
    public:
        FExampleSpace* base_example_space;
        PEnv env;
        int rewrite_id;

        IncreExampleList example_list;
        TypeList inp_type_list;
        Ty oup_ty;
        std::vector<FRes> f_res_list;
        TypedProgramList extract_program_list;
        int KExampleTimeOut = 10, current_pos, KExampleEnlargeFactor = 2;

        std::vector<std::pair<AuxProgram, DataList*>> inp_cache_list;
        std::vector<std::pair<std::pair<PProgram, std::vector<int>>, DataList*>> oup_cache_list;

        void insertExample(const IncreExample& example) {
            example_list.push_back(example);
        }
        Data runOutput(const Ty& type, int example_id, std::vector<int>& path, int& cache_id) {
            if (type->getType() == TypeType::TUPLE) {
                auto* tt = dynamic_cast<TyTuple*>(type.get());
                assert(tt);
                DataList elements(tt->fields.size());
                for (int i = 0; i < tt->fields.size(); ++i) {
                    path.push_back(i);
                    elements[i] = runOutput(tt->fields[i], example_id, path, cache_id);
                    path.pop_back();
                }
                return BuildData(Product, elements);
            }
            if (type->getType() == TypeType::COMPRESS) {
                auto* ct = dynamic_cast<TyLabeledCompress*>(type.get());
                assert(ct); int num = f_res_list[ct->id].component_list.size();
                if (!num) return Data(std::make_shared<VUnit>());
                if (num == 1) return oup_cache_list[cache_id++].second->at(example_id);
                DataList elements(num);
                for (int i = 0; i < num; ++i) elements[i] = oup_cache_list[cache_id++].second->at(example_id);
                return Data(std::make_shared<VTuple>(elements));
            }
            return oup_cache_list[cache_id++].second->at(example_id);
        }
        void buildExample(int example_id) {
#ifdef DEBUG
            assert(example_id < base_example_space->example_list.size());
#endif
            DataList inp(inp_cache_list.size());
            for (int i = 0; i < inp_cache_list.size(); ++i) {
                inp[i] = inp_cache_list[i].second->at(example_id);
            }

            std::vector<int> path; int cache_id = 0;
            auto oup = runOutput(oup_ty, example_id, path, cache_id);
            insertExample(std::make_shared<IncreExampleData>(rewrite_id, inp, base_example_space->example_list[example_id]->global_inputs, oup));
        }

        void collectOutputCache(const Ty& type, std::vector<int>& path) {
            if (type->getType() == TypeType::TUPLE) {
                auto* tt = dynamic_cast<TyTuple*>(type.get());
                for (int i = 0; i < tt->fields.size(); ++i) {
                    path.push_back(i);
                    collectOutputCache(tt->fields[i], path);
                    path.pop_back();
                }
                return;
            }
            if (type->getType() == TypeType::COMPRESS) {
                auto* ct = dynamic_cast<TyLabeledCompress*>(type.get());
                for (auto& component: f_res_list[ct->id].component_list) {
                    auto* cache = base_example_space->getOupCache(component.program.second, path, 0);
                    if (!cache) {
                        LOG(INFO) << "Unknown cache";
                        base_example_space->registerOupCache(component.program.second, path, {});
                        cache = base_example_space->getOupCache(component.program.second, path, 0);
                    }
                    oup_cache_list.emplace_back(std::make_pair(component.program.second, path), cache);
                }
                return;
            }
            auto* cache = base_example_space->getOupCache(nullptr, path, 0); assert(cache);
            oup_cache_list.emplace_back(std::make_pair(PProgram(nullptr), path), cache);
        }
        void extendCache(int target_num) {
            for (auto& [aux, cache_item]: inp_cache_list) base_example_space->extendAuxCache(aux, cache_item, target_num);
            for (auto& [comp, cache_item]: oup_cache_list) base_example_space->extendOupCache(comp.first, comp.second, cache_item, target_num);
        }
        CExampleSpace(int _rewrite_id, FExampleSpace* _base_example_space, IncreAutoLifterSolver* source):
                rewrite_id(_rewrite_id), base_example_space(_base_example_space) {
            env = source->env; f_res_list = source->f_res_list;
            oup_ty = source->info->rewrite_info_list[rewrite_id].oup_type;
            extract_program_list = source->extract_res_list[rewrite_id].compress_list;

            int init_example_num = base_example_space->example_list.size();
            current_pos = (init_example_num + 1) / KExampleEnlargeFactor;

            // collect input types
            for (auto& [compress_type, compress_program]: extract_program_list) {
                auto* ltc = dynamic_cast<incre::trans::TLabeledCompress*>(compress_type.get());
                if (!ltc) {
                    inp_type_list.push_back(compress_type);
                } else {
                    for (auto& aux_program: f_res_list[ltc->id].component_list) {
                        inp_type_list.push_back(aux_program.program.first);
                    }
                }
            }

            // Initialize cache list
            for (auto& compress_program: extract_program_list) {
                auto compress_type = compress_program.first;
                auto* ltc = dynamic_cast<incre::trans::TLabeledCompress*>(compress_type.get());
                if (!ltc) {
                    AuxProgram aux = {compress_program, {nullptr, nullptr}};
                    auto* cache_item = base_example_space->getAuxCache(aux, 0);
                    if (!cache_item) {
                        LOG(INFO) << "Unknown cache for " << aux2String(aux);
                        base_example_space->registerAuxCache(aux, {});
                        cache_item = base_example_space->getAuxCache(aux, 0);
                    }
                    inp_cache_list.emplace_back(aux, cache_item);
                } else {
                    for (auto& aux_program: f_res_list[ltc->id].component_list) {
                        AuxProgram aux = {compress_program, aux_program.program};
                        auto* cache_item = base_example_space->getAuxCache(aux, 0);
                        if (!cache_item) {
                            LOG(INFO) << "Unknown cache for " << aux2String(aux);
                            base_example_space->registerAuxCache(aux, {});
                            cache_item = base_example_space->getAuxCache(aux, 0);
                        }
                        inp_cache_list.emplace_back(aux, cache_item);
                    }
                }
            }
            std::vector<int> path;
            collectOutputCache(oup_ty, path);
            //LOG(INFO) << "build " << oup_ty->toString() << " " << oup_cache_list.size();

            extendCache(current_pos);
            for (int i = 0; i < current_pos; ++i) {
                buildExample(i);
            }
        }

        int extendExample() {
            int target_num = current_pos * KExampleEnlargeFactor;
            auto* guard = new TimeGuard(KExampleTimeOut);
            target_num = base_example_space->acquireExample(target_num, guard);
            extendCache(target_num);
            delete guard;
            for (;current_pos < target_num; ++current_pos) {
                buildExample(current_pos);
            }
            return target_num;
        }

        bool isValid(const PProgram& program) {
            for (auto& example: example_list) {
                if (!(env->run(program.get(), data::concatDataList(example->local_inputs, example->global_inputs)) == example->oup)) {
                    return false;
                }
            }
            return true;
        }
    };

    PProgram _mergeComponentProgram(TypeData* type, int& pos, const ProgramList& program_list, const std::vector<FRes>& f_res_list, Env* env) {
        if (type->getType() == TypeType::COMPRESS) {
            auto* ct = dynamic_cast<TyLabeledCompress*>(type); assert(ct);
            int size = f_res_list[ct->id].component_list.size();
            if (size == 0) return program::buildConst(Data(std::make_shared<VUnit>()));
            if (size == 1) return program_list[pos++];
            ProgramList sub_list;
            for (int i = 0; i < size; ++i) sub_list.push_back(program_list[pos++]);
            return std::make_shared<Program>(env->getSemantics("prod"), sub_list);
        }
        if (type->getType() == TypeType::TUPLE) {
            auto* tt = dynamic_cast<TyTuple*>(type);
            ProgramList sub_list;
            for (auto& sub: tt->fields) {
                sub_list.push_back(_mergeComponentProgram(sub.get(), pos, program_list, f_res_list, env));
            }
            return std::make_shared<Program>(env->getSemantics("prod"), sub_list);
        }
        return program_list[pos++];
    }

    PProgram _mergeComponentProgram(TypeData* type, const ProgramList& program_list, const std::vector<FRes>& f_res_list, Env* env) {
        int pos = 0;
        auto res = _mergeComponentProgram(type, pos, program_list, f_res_list, env);
        assert(pos == program_list.size());
        return res;
    }

    std::string _path2String(const std::vector<int>& path) {
        std::string res = "[";
        for (int i = 0; i < path.size(); ++i) {
            if (i) res += ","; res += std::to_string(path[i]);
        }
        return res + "]";
    }

    RelatedComponents _getComponentList(const std::map<std::vector<int>, std::vector<RelatedComponents>>& records,
                                        const std::vector<int>& path) {
        {
            auto it = records.find(path);
            if (it != records.end()) {
                assert(it->second.size() == 1);
                return it->second[0];
            }
        }
        assert(!path.empty()); int size = path.size();
        auto new_path = path; int last = new_path[size - 1]; new_path.pop_back();
        auto it = records.find(new_path);
        assert(it != records.end() && it->second.size() > last);
        return it->second[last];
    }

    PProgram _synthesisCombinator(CExampleSpace* example_space, const std::vector<_OutputCase>& component_info_list, IncreAutoLifterSolver* solver,
                                  const std::map<std::vector<int>, std::vector<RelatedComponents>>& records, const std::vector<std::pair<int, int>>& param_list) {
        ProgramList res_list;
        for (auto& component_info: component_info_list) {
            IOExampleList component_example_list;
            for (auto& example: example_space->example_list) {
                auto oup_component = component_info.extract(example->oup, example->global_inputs);
                component_example_list.emplace_back(example->local_inputs, oup_component);
            }

            // synthesis
            auto oup_type = component_info.program.first;
            auto* grammar = solver->buildCombinatorGrammar(example_space->inp_type_list, oup_type, example_space->rewrite_id);

            auto main = autolifter::util::synthesis2Program(example_space->inp_type_list, grammar->start->type, example_space->env, grammar, component_example_list);
            res_list.push_back(main);
        }
        return _mergeComponentProgram(example_space->oup_ty.get(), res_list, example_space->f_res_list, example_space->env.get());
    }
}

namespace {
    Term _buildSingleStep(const PSemantics& sem, const incre::grammar::SynthesisComponentList& component_list, const TermList& sub_list) {
        for (auto& component: component_list) {
            auto res = component->tryBuildTerm(sem, sub_list);
            if (res) return res;
        }
        if (sem->getName() == "unit" || sem->getName() == "Unit") {
            auto* cs = dynamic_cast<ConstSemantics*>(sem.get());
            assert(cs);
            return std::make_shared<TmValue>(cs->w);
        }
        LOG(FATAL) << "Cannot build IncreTerm for semantics " << sem->getName();
    }

    Term _buildProgram(Program* program, const incre::grammar::SynthesisComponentList& component_list, const TermList& param_list) {
        auto *ps = dynamic_cast<ParamSemantics *>(program->semantics.get());
        if (ps) return param_list[ps->id];
        TermList sub_list;
        for (const auto& sub: program->sub_list) sub_list.push_back(_buildProgram(sub.get(), component_list, param_list));
        return _buildSingleStep(program->semantics, component_list, sub_list);
    }

    bool _isSymbolTerm(TermData* term) {
        return term->getType() == TermType::VALUE || term->getType() == TermType::VAR;
    }
}

Term IncreAutoLifterSolver::synthesisCombinator(int rewrite_id) {
    auto* example_space = new CExampleSpace(rewrite_id, example_space_list[rewrite_id], this);
    auto output_cases = _collectOutputCase(rewrite_id, this);
    {
        LOG(INFO) << "Synthesize for rewrite@" << rewrite_id;
        LOG(INFO) << "Output cases " << output_cases.size();
        for (auto &component: output_cases) {
            std::cout << "  " << _path2String(component.path) << " ";
            if (!component.program.second) std::cout << "null" << std::endl;
            else std::cout << component.program.second->toString() << std::endl;
        }
        LOG(INFO) << "Input list " << example_space->extract_program_list.size();
        int input_id = 0;
        for (auto &[type, prog]: example_space->extract_program_list) {
            auto *ltc = dynamic_cast<incre::trans::TLabeledCompress*>(type.get());
            if (ltc) {
                LOG(INFO) << "compress input " << ltc->id;
                for (auto &align_prog: f_res_list[ltc->id].component_list) {
                    LOG(INFO) << "  [" << input_id++ << "] " << prog->toString() << " -> " << align_prog.program.second->toString() << std::endl;
                }
            } else {
                LOG(INFO) << "  [" << input_id++ << "] " << prog->toString() << std::endl;
            }
        }
    }

    PProgram res = nullptr;
    std::vector<std::pair<int, int>> param_positions;
    for (int i = 0; i < extract_res_list[rewrite_id].compress_list.size(); ++i) {
        auto* lt = dynamic_cast<incre::trans::TLabeledCompress*>(extract_res_list[rewrite_id].compress_list[i].first.get());
        if (lt) {
            auto& f_res = f_res_list[lt->id];
            for (int j = 0; j < f_res.component_list.size(); ++j) param_positions.emplace_back(i, j);
        } else param_positions.emplace_back(i, -1);
    }

    while (!res || !example_space->isValid(res)) {
        res = _synthesisCombinator(example_space, output_cases, this, rewrite_result_records[rewrite_id], param_positions);
        example_space->extendExample();
    }

    // Build Param List
    TermList extract_param_list;
    for (auto& [var_name, var_type]: info->rewrite_info_list[rewrite_id].inp_types) {
        extract_param_list.push_back(std::make_shared<TmVar>(var_name));
    }
    for (auto& var_name: info->example_pool->global_name_list) {
        extract_param_list.push_back(std::make_shared<TmVar>(var_name));
    }

    std::vector<std::pair<std::string, Term>> binding_list;
    TermList combine_param_list;
    int var_id = 0;

    for (auto& [extract_type, extract_program]: extract_res_list[rewrite_id].compress_list) {
        // LOG(INFO) << "build compress " << compress_program->toString();
        // for (auto& param: compress_param_list) LOG(INFO) << "  param: " << param->toString();
        auto extract_term = _buildProgram(extract_program.get(), info->component_pool.extract_list, extract_param_list);
        if (!_isSymbolTerm(extract_term.get())) {
            std::string extract_name = "c" + std::to_string(var_id++);
            binding_list.emplace_back(extract_name, extract_term);
            extract_term = std::make_shared<TmVar>(extract_name);
        }

        auto* lt = dynamic_cast<incre::trans::TLabeledCompress*>(extract_type.get());
        if (!lt) {
            combine_param_list.push_back(extract_term);
        } else {
            int compress_id = lt->id;
            int component_num = f_res_list[compress_id].component_list.size();
            if (!component_num) continue;
            if (component_num == 1) {
                combine_param_list.push_back(extract_term);
            } else {
                for (int i = 1; i <= component_num; ++i) {
                    combine_param_list.push_back(std::make_shared<TmProj>(extract_term, i, component_num));
                }
            }
        }
    }

    auto term = _buildProgram(res.get(), info->component_pool.comb_list, combine_param_list);
    std::reverse(binding_list.begin(), binding_list.end());
    for (auto& [name, binding]: binding_list) {
        term = std::make_shared<TmLet>(name, false, binding, term);
    }

    return term;
}

#include "istool/basic/config.h"

void IncreAutoLifterSolver::solveCombinators() {
    for (int pass_id = 0; pass_id < info->rewrite_info_list.size(); ++pass_id) {
        global::printStageResult("  Synthesizing sketch hole " + std::to_string(pass_id + 1) + "/" + std::to_string(info->rewrite_info_list.size()));
        comb_list.push_back(synthesisCombinator(pass_id));
    }
    /*auto comb_size = 0;
    for (auto& comb: comb_list) comb_size += incre::getTermSize(comb.get());
    global::recorder.record("comb-size", comb_size);*/
}

namespace {
    PType _getCompressType(analysis::IncreInfoData *info, int compress_id) {
        for (const auto &rewrite_info: info->rewrite_info_list) {
            for (auto &[name, ty]: rewrite_info.inp_types) {
                if (ty->getType() == TypeType::COMPRESS) {
                    auto *cty = dynamic_cast<TyLabeledCompress *>(ty.get());
                    if (cty && cty->id == compress_id) return incre::trans::typeFromIncre(cty->body.get());
                }
            }
        }
        LOG(FATAL) << "Compress #" << compress_id << " not found";
    }
}

TermList IncreAutoLifterSolver::buildFRes() {
    TermList result;
    for (int compress_id = 0; compress_id < f_res_list.size(); ++compress_id) {
        auto compress_type = _getCompressType(info.get(), compress_id);
        std::string compress_name = "ds";
        TermList param_list; param_list.push_back(std::make_shared<TmVar>(compress_name));
        for (auto& var_name: info->example_pool->global_name_list) {
            param_list.push_back(std::make_shared<TmVar>(var_name));
        }

        TermList fields;
        for (auto& component_info: f_res_list[compress_id].component_list) {
            LOG(INFO) << component_info.program.second->toString();
            fields.push_back(_buildProgram(component_info.program.second.get(), info->component_pool.comp_list, param_list));
        }

        if (fields.empty()) {
            auto data = Data(std::make_shared<VUnit>());
            result.push_back(std::make_shared<TmValue>(data));
        } else if (fields.size() == 1) {
            result.push_back(std::make_shared<TmFunc>(compress_name, fields[0]));
        } else {
            auto res = std::make_shared<TmTuple>(fields);
            result.push_back(std::make_shared<TmFunc>(compress_name, res));
        }
    }
    return result;
}