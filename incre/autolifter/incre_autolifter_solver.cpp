//
// Created by pro on 2022/9/26.
//

#include <istool/ext/deepcoder/data_type.h>
#include <istool/ext/deepcoder/deepcoder_semantics.h>
#include "istool/basic/config.h"
#include "istool/incre/autolifter/incre_autolifter_solver.h"
#include "istool/incre/autolifter/incre_plp_solver.h"
#include "istool/incre/trans/incre_trans.h"
#include "istool/solver/autolifter/basic/streamed_example_space.h"
#include "glog/logging.h"
#include <iostream>

using namespace incre;
using namespace incre::autolifter;
using namespace incre::syntax;
using namespace incre::semantics;
using namespace incre::analysis;

FInfo::FInfo(const TypedProgram& _program, int _id, bool _is_extended):
        program(_program), id(_id), is_extended(_is_extended) {
}

namespace {
    int _getCNum(IncreInfoData* info) {
        int c_num = 0;
        auto get_id = [](syntax::TypeData* type) {
            auto* ct = dynamic_cast<TyLabeledCompress*>(type);
            if (ct) return ct->id + 1;
            return 0;
        };
        for (auto& rewrite_info: info->rewrite_info_list) {
            for (auto& [_, type]: rewrite_info.inp_types) c_num = std::max(c_num, get_id(type.get()));
            c_num = std::max(c_num, get_id(rewrite_info.oup_type.get()));
        }
        return c_num;
    }

    void _unfoldOutputType(const Ty& type, std::vector<int>& path, std::vector<OutputUnit>& res) {
        if (type->getType() == TypeType::TUPLE) {
            auto* tt = dynamic_cast<TyTuple*>(type.get());
            for (int i = 0; i < tt->fields.size(); ++i) {
                path.push_back(i);
                _unfoldOutputType(tt->fields[i], path, res);
                path.pop_back();
            }
        } else {
            res.emplace_back(path, type);
        }
    }

    std::vector<OutputUnit> _unfoldOutputType(const Ty& type) {
        std::vector<OutputUnit> res;
        std::vector<int> path;
        _unfoldOutputType(type, path, res);
        return res;
    }

    PType _getCompressType(IncreInfoData* info, int compress_id) {
        for (const auto& rewrite_info: info->rewrite_info_list) {
            for (auto&[name, ty]: rewrite_info.inp_types) {
                if (ty->getType() == TypeType::COMPRESS) {
                    auto *cty = dynamic_cast<TyLabeledCompress *>(ty.get());
                    if (cty && cty->id == compress_id) return incre::trans::typeFromIncre(cty->body.get());
                }
            }
        }
        LOG(FATAL) << "Compress #" << compress_id << " not found";
    }
}

OutputUnit::OutputUnit(const std::vector<int> &_path, const Ty &_unit_type): path(_path), unit_type(_unit_type) {
}

GrammarEnumerateTool::GrammarEnumerateTool(Grammar *_grammar): grammar(_grammar), size_limit(::grammar::getMaxSize(_grammar)) {
    if (size_limit == -1) size_limit = 1e9;
}

Grammar * IncreAutoLifterSolver::buildCompressGrammar(int compress_id) {
    TypeList inp_list = {_getCompressType(info.get(), compress_id)};
    for (auto& inp_type: global_input_type_list) inp_list.push_back(inp_type);
    return info->component_pool.buildCompGrammar(inp_list);
}
Grammar * IncreAutoLifterSolver::buildExtractGrammar(const TypeList &type_list, int align_id) {
    int pos = info->rewrite_info_list[align_id].command_id;
    TypeList inp_list = type_list;
    for (auto& inp_type: global_input_type_list) inp_list.push_back(inp_type);
    return info->component_pool.buildExtractGrammar(inp_list, pos);
}
Grammar * IncreAutoLifterSolver::buildCombinatorGrammar(const TypeList &type_list, const PType& oup_type, int align_id) {
    auto feature = std::to_string(align_id) + "@" + type::typeList2String(type_list) + "@" + oup_type->getName();
    if (combine_grammar_map.count(feature)) return combine_grammar_map[feature];
    int pos = info->rewrite_info_list[align_id].command_id;
    return info->component_pool.buildCombGrammar(type_list, oup_type, pos);
}

IncreAutoLifterSolver::IncreAutoLifterSolver(const analysis::IncreInfo& _info, const PEnv &_env): IncreSolver(_info), env(_env),
    f_res_list(_getCNum(_info.get())), extract_res_list(_info->rewrite_info_list.size()), rewrite_result_records(info->rewrite_info_list.size()) {
    for (auto& global_type: info->example_pool->global_type_list) {
        global_input_type_list.push_back(trans::typeFromIncre(global_type.get()));
    }

    for (auto& rewrite_info: info->rewrite_info_list) {
        assert(rewrite_info.index == example_space_list.size());
        auto* example_space = new FExampleSpace(info->example_pool, rewrite_info.index, env, rewrite_info);
        example_space_list.push_back(example_space);
        unit_storage.push_back(_unfoldOutputType(rewrite_info.oup_type));

        extract_grammar_list.push_back(new GrammarEnumerateTool(buildExtractGrammar(example_space->local_types, rewrite_info.index)));
    }
    for (int i = 0; i < f_res_list.size(); ++i) {
        compress_grammar_list.push_back(new GrammarEnumerateTool(buildCompressGrammar(i)));
    }

    /*
    LOG(INFO) << "print grammar";
    int num = 0;
    for (auto& grammar_enum: compress_grammar_list) {
        std::cout << "num = " << num++ << std::endl;
        auto& grammar = grammar_enum->grammar;
        grammar->print();
    }
    num = 0;
    for (auto& grammar_enum: aux_grammar_list) {
        std::cout << "num = " << num++ << std::endl;
        auto& grammar = grammar_enum->grammar;
        grammar->print();
    }
    for (auto& [name, grammar]: combine_grammar_map) {
        std::cout << name << " " << std::endl;
        grammar->print();
    }*/
#ifdef DEBUG
    for (int i = 0; i < info->rewrite_info_list.size(); ++i) assert(info->rewrite_info_list[i].index == i);
#endif
}
IncreAutoLifterSolver::~IncreAutoLifterSolver() {
    for (auto* example_space: example_space_list) {
        delete example_space;
    }
    for (auto* g: compress_grammar_list) delete g;
    for (auto* g: extract_grammar_list) delete g;
    for (auto& [_, grammar]: combine_grammar_map) delete grammar;
}
bool FRes::isEqual(Program *x, Program *y) {
    //TODO: add a semantical check
    return x->toString() == y->toString();
}
int FRes::insert(const TypedProgram& program) {
    for (int i = 0; i < component_list.size(); ++i) {
        auto& info = component_list[i];
        if (type::equal(program.first, info.program.first) && isEqual(program.second.get(), info.program.second.get())) return i;
    }
    int id = component_list.size();
    component_list.emplace_back(program, id, false);
    return id;
}

bool CompressRes::isEqual(Program* x, Program* y) {
    // TODO: add a semantical check
    return x->toString() == y->toString();
}
int CompressRes::insert(const TypedProgram& program) {
    for (int i = 0; i < compress_list.size(); ++i) {
        if (type::equal(program.first, compress_list[i].first) && isEqual(program.second.get(), compress_list[i].second.get())) return i;
    }
    int id = int(compress_list.size()); compress_list.push_back(program);
    return id;
}

namespace {
    int _getCompressId(const Ty& type) {
        auto* tc = dynamic_cast<TyLabeledCompress*>(type.get());
        if (tc) return tc->id;
        return -1;
    }
}

namespace {
    int _getIncreTermSize(Program* program) {
        int res = program->sub_list.size() + 1;
        if (program->semantics->getName() == "prod") res = 1;
        if (program->semantics->getName().substr(0, 6) == "access") res = 1;
        for (auto& sub_program: program->sub_list) {
            res += _getIncreTermSize(sub_program.get());
        }
        return res;
    }
}

autolifter::PLPRes IncreAutoLifterSolver::solvePLPTask(const RewriteTypeInfo& info, const TypedProgram &target, const OutputUnit& unit) {
    auto* space = example_space_list[info.index];

    std::vector<TypedProgramList> known_lifts(f_res_list.size());
    for (int i = 0; i < f_res_list.size(); ++i) {
        for (auto& component: f_res_list[i].component_list) {
            known_lifts[i].push_back(component.program);
        }
    }

    auto* task = new PLPTask(space, compress_grammar_list, known_lifts, extract_grammar_list[info.index], target, unit.path, _getCompressId(unit.unit_type));
    auto* solver = new IncrePLPSolver(env.get(), task);
    auto res = solver->synthesis(nullptr);

    delete solver; delete task;
    return res;
}

void IncreAutoLifterSolver::solveAuxiliaryProgram() {
    auto record_res = [&](int rewrite_id, const PLPRes& res, const std::vector<int>& path) {
        RelatedComponents related;
        for (auto& [compress_program, aux_program]: res) {
            auto* ltc = dynamic_cast<incre::trans::TLabeledCompress*>(compress_program.first.get());
            int aux_id = -1, compress_id = -1;
            if (ltc) {
                aux_id = f_res_list[ltc->id].insert(aux_program);
            }
            compress_id = extract_res_list[rewrite_id].insert(compress_program);
            related.emplace_back(compress_id, aux_id);
        }
        return related;
    };

    for (auto& rewrite_info: info->rewrite_info_list) {
        unit_storage.push_back(_unfoldOutputType(rewrite_info.oup_type));
    }

    {
        global::printStageResult("  Iteration #0");
        int sub_task_num = 0;
        for (auto& unit_list: unit_storage) {
            for (auto& unit: unit_list) {
                auto* oup_ty = unit.unit_type.get();
                if (!dynamic_cast<TyCompress*>(oup_ty)) sub_task_num += 1;
            }
        }

        int sub_task_id = 0;
        for (auto &rewrite_info: info->rewrite_info_list) {
            for (auto &unit: unit_storage[rewrite_info.index]) {
                auto *oup_ty = unit.unit_type.get();
                if (!dynamic_cast<TyCompress *>(oup_ty)) {
                    global::printStageResult("    Solving subtask " + std::to_string(++sub_task_id) + "/" +
                                             std::to_string(sub_task_num));
                    PLPRes res = solvePLPTask(rewrite_info, {incre::trans::typeFromIncre(unit.unit_type.get()), nullptr}, unit);
                    auto related = record_res(rewrite_info.index, res, unit.path);

                    rewrite_result_records[rewrite_info.index][unit.path].push_back(related);
                }
            }
        }
    }

    bool is_changed = true;
    int iteration_id = 0;
    while (is_changed) {
        is_changed = false;
        global::printStageResult("  Iteration #" + std::to_string(++iteration_id));
        int sub_task_num = 0; std::vector<int> extend_limit;
        for (const auto& f_res: f_res_list) {
            extend_limit.push_back(f_res.component_list.size());
            for (const auto& component: f_res.component_list) {
                if (!component.is_extended) sub_task_num++;
            }
        }
        int sub_task_id = 0;
        for (int compress_id = 0; compress_id < f_res_list.size(); ++compress_id) {
            for (int i = 0; i < extend_limit[compress_id]; ++i) {
                auto component = f_res_list[compress_id].component_list[i];
                if (component.is_extended) continue;
                global::printStageResult("    Solving subtask " + std::to_string(++sub_task_id) + "/" + std::to_string(sub_task_num));
                is_changed = true;
                f_res_list[compress_id].component_list[i].is_extended = true;
                for (auto& rewrite_info: info->rewrite_info_list) {
                    int index = rewrite_info.index;
                    for (auto& unit: unit_storage[index]) {
                        auto* cty = dynamic_cast<TyLabeledCompress*>(unit.unit_type.get());
                        if (cty && cty->id == compress_id) {
                            PLPRes res = solvePLPTask(rewrite_info, component.program, unit);
                            auto related = record_res(index, res, unit.path);
                            rewrite_result_records[index][unit.path].push_back(related);
                        }
                    }
                }
            }
        }
    }

    // build type list
    for (auto& f_res: f_res_list) {
        if (f_res.component_list.empty()) {
            f_type_list.push_back(std::make_shared<TyUnit>()); continue;
        }
        TyList fields;
        for (auto& component: f_res.component_list) fields.push_back(incre::trans::typeToIncre(component.program.first.get()));
        if (fields.size() == 1) {
            f_type_list.push_back(fields[0]);
        } else {
            f_type_list.push_back(std::make_shared<TyTuple>(fields));
        }
    }
}

#include "istool/basic/config.h"

IncreSolution IncreAutoLifterSolver::solve() {
    global::printStageResult("Stage 1/2: synthesizing the representation function.");
    solveAuxiliaryProgram();
    global::printStageResult("Stage 2/2: synthesizing the combinator.");
    solveCombinators();
    if (env->getConstRef(config::KPrintAlignName, BuildData(Bool, false))->isTrue()) {
        auto repr_list = buildFRes();
        return {f_type_list, comb_list, repr_list};
    }
    return {f_type_list, comb_list, {}};
}