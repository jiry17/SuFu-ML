//
// Created by pro on 2022/9/26.
//

#ifndef ISTOOL_INCRE_AUTOLIFTER_SOLVER_H
#define ISTOOL_INCRE_AUTOLIFTER_SOLVER_H

#include "istool/incre/incre_solver.h"
#include "istool/basic/grammar.h"
#include "istool/basic/bitset.h"
#include "incre_plp.h"
#include <map>

namespace incre {
    namespace autolifter {
        struct FInfo {
            TypedProgram program;
            int id;
            bool is_extended;
            FInfo(const TypedProgram& _program, int _id, bool _is_extended);
        };

        struct FRes {
        private:
            bool isEqual(Program* x, Program* y);
        public:
            std::vector<FInfo> component_list;
            int insert(const TypedProgram& program);
        };

        struct CompressRes {
        private:
            bool isEqual(Program* x, Program* y);
        public:
            std::vector<TypedProgram> compress_list;
            int insert(const TypedProgram& program);
        };

        struct OutputUnit {
            std::vector<int> path;
            syntax::Ty unit_type;
            OutputUnit(const std::vector<int>& _path, const syntax::Ty& _unit_type);
        };

        typedef std::vector<std::pair<int, int>> RelatedComponents;

    }
    class IncreAutoLifterSolver: public IncreSolver {
        // Grammar builder
        std::vector<autolifter::GrammarEnumerateTool*> extract_grammar_list, compress_grammar_list;
        std::unordered_map<std::string, Grammar*> combine_grammar_map;
        autolifter::PLPRes solvePLPTask(const analysis::RewriteTypeInfo& info, const autolifter::TypedProgram& target, const autolifter::OutputUnit& unit);
        Grammar* buildCompressGrammar(int compress_id);
        Grammar* buildExtractGrammar(const TypeList& type_list, int align_id);
    public:
        Grammar* buildCombinatorGrammar(const TypeList& type_list, const PType& oup_type, int align_id);

        PEnv env;
        std::vector<autolifter::FExampleSpace*> example_space_list;
        TypeList global_input_type_list;
        std::vector<std::vector<autolifter::OutputUnit>> unit_storage;
        std::vector<std::map<std::vector<int>, std::vector<autolifter::RelatedComponents>>> rewrite_result_records;

        IncreAutoLifterSolver(const analysis::IncreInfo& _info, const PEnv& _env);
        virtual ~IncreAutoLifterSolver();
        virtual IncreSolution solve();

        // Synthesize auxiliary programs
        void solveAuxiliaryProgram();
        std::vector<autolifter::FRes> f_res_list;
        std::vector<autolifter::CompressRes> extract_res_list;
        syntax::TyList f_type_list;

        // Synthesize combinators
        syntax::Term synthesisCombinator(int align_id);
        syntax::TermList comb_list;
        void solveCombinators();
        syntax::TermList buildFRes();
    };
}

#endif //ISTOOL_INCRE_AUTOLIFTER_SOLVER_H
