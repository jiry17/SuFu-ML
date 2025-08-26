//
// Created by pro on 2024/3/30.
//

#include "istool/incre/io/incre_printer.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::io;
using namespace incre::syntax;

const int io::KLineWidth = 50;
const int io::KIndent = 2;

void OutputResult::addIndent(int start) {
    for (int i = start; i < output_list.size(); ++i) {
        output_list[i] = std::string(KIndent, ' ') + output_list[i];
    }
}

std::string OutputResult::toString() const {
    std::string res;
    for (auto& s: output_list) res += s + "\n";
    return res;
}

void OutputResult::pushStart(const std::string &s) {
    output_list[0] = s + output_list[0];
}

void OutputResult::appendLast(const std::string &s) {
    auto n = output_list.size();
    output_list[n - 1] += s;
}

OutputResult::OutputResult(const std::string& s): output_list({s}) {}
OutputResult::OutputResult(const std::vector<std::string>& _output_list): output_list(_output_list) {}

std::string io::pattern2String(syntax::PatternData *pattern) {
    switch (pattern->getType()) {
        case PatternType::UNDERSCORE: return "_";
        case PatternType::TUPLE: {
            auto* pt = dynamic_cast<PtTuple*>(pattern);
            std::string res = "{";
            for (int i = 0; i < pt->fields.size(); ++i) {
                if (i) res += ", "; res += pattern2String(pt->fields[i].get());
            }
            return res + "}";
        }
        case PatternType::VAR: {
            auto* pv = dynamic_cast<PtVar*>(pattern);
            if (!pv->body) return pv->name;
            return "(" + pattern2String(pv->body.get()) + ")@" + pv->name;
        }
        case PatternType::CONS: {
            auto* pc = dynamic_cast<PtCons*>(pattern);
            return pc->name + " " + pattern2String(pc->body.get());
        }
    }
}

namespace {
    int KIsHighlightRewrite = false;

    OutputResult _aTerm2String(TermData*);
    OutputResult _term2String(TermData*, bool);
    OutputResult _funcDef2String(TermData*, const std::string& linker, bool);
    OutputResult _appTerm2String(TermData*);
    std::pair<int, OutputResult> _arithTerm2String(TermData*);

    OutputResult _pathTerm2String(TermData* term) {
        switch (term->getType()) {
            case TermType::PROJ: {
                auto* tp = dynamic_cast<TmProj*>(term);
                if (tp->size == 2) {
                    auto tmp_name = tp->id == 1 ? "fst" : "snd";
                    auto tmp_term = std::make_shared<TmApp>(std::make_shared<TmVar>(tmp_name), tp->body);
                    return _pathTerm2String(tmp_term.get());
                }
                auto res = _pathTerm2String(tp->body.get());
                res.appendLast("[" + std::to_string(tp->id) + "/" + std::to_string(tp->size) + "]");
                return res;
            }
            default: return _aTerm2String(term);
        }
    }

    OutputResult concat(const OutputResult& l, const OutputResult& r) {
        std::vector<std::string> full_result = l.output_list;
        for (auto& line: r.output_list) full_result.push_back(line);
        return {full_result};
    }

    OutputResult defaultMerge(const OutputResult& l, const OutputResult& r) {
        if (l.output_list.size() == 1 && r.output_list.size() == 1 && l.output_list[0].length() + r.output_list[0].length() + 1 < KLineWidth) {
            return {l.output_list[0] + " " + r.output_list[0]};
        }
        return concat(l, r);
    }

    OutputResult betterChoice(const OutputResult& l, const OutputResult& r) {
        if (l.output_list.size() <= r.output_list.size()) return l; else return r;
    }

    OutputResult _matchCase2String(TmMatch* tm) {
        OutputResult res;
        for (auto& [pt, case_term]: tm->cases) {
            auto pattern = pattern2String(pt.get());
            auto case_res = _term2String(case_term.get(), true);
            case_res = defaultMerge(pattern + " ->", case_res);
            case_res.addIndent(); case_res.output_list[0][0] = '|';
            res = concat(res, case_res);
        }
        return res;
    }

    OutputResult _term2String(TermData* term, bool is_case_end) {
        switch (term->getType()) {
            case TermType::IF: {
                auto* ti = dynamic_cast<TmIf*>(term);
                auto c_res = _term2String(ti->c.get(), false);
                auto t_res = _term2String(ti->t.get(), false);
                auto f_res = _term2String(ti->f.get(), is_case_end);

                c_res.pushStart("if "); c_res.addIndent(1);
                t_res.pushStart("then "); t_res.addIndent(1);
                f_res.pushStart("else "); f_res.addIndent(1);

                return defaultMerge(c_res, defaultMerge(t_res, f_res));
            }
            case TermType::LET: {
                auto* tl = dynamic_cast<TmLet*>(term);
                auto def_res = _funcDef2String(tl->def.get(), "=", false);
                def_res.pushStart(tl->name);
                auto body_res = _term2String(tl->body.get(), is_case_end);

                if (tl->is_rec) def_res.pushStart("let rec "); else def_res.pushStart("let ");
                def_res.appendLast(" in");
                return concat(def_res, body_res);
            }
            case TermType::FUNC: {
                auto res = _funcDef2String(term, "->", is_case_end);
                res.pushStart("fun");
                return res;
            }
            case TermType::MATCH: {
                auto* tm = dynamic_cast<TmMatch*>(term);
                auto res = _term2String(tm->def.get(), false);
                res.pushStart("match "); res.addIndent(1);
                res.appendLast(" with");
                res = concat(res, _matchCase2String(tm));
                if (is_case_end) {
                    res.pushStart("("); res.appendLast(")");
                }
                return res;
            }
            default: return _arithTerm2String(term).second;
        }
    }

    const std::unordered_map<std::string, std::pair<int, std::vector<int>>> KPriorityMap = {
            {"or",  {0, {0, 0}}},
            {"and", {1, {1, 1}}},
            {"not", {2, {2}}},
            {"<=",  {3, {4, 4}}},
            {"<",   {3, {4, 4}}},
            {">",   {3, {4, 4}}},
            {"==",  {3, {4, 4}}},
            {">=",  {3, {4, 4}}},
            {"+",   {4, {4, 4}}},
            {"-",   {4, {4, 5}}},
            {"neg", {5, {5}}},
            {"*",   {6, {6, 6}}},
            {"/",   {6, {6, 7}}}
    };

    std::pair<int, std::vector<int>> _getPriority(const std::string& op) {
        auto it = KPriorityMap.find(op);
        if (it == KPriorityMap.end()) LOG(FATAL) << "Unknown operator " << op;
        return it->second;
    }

    std::pair<int, OutputResult> _arithTerm2String(TermData* term) {
        switch (term->getType()) {
            case TermType::PRIMARY: {
                auto* tp = dynamic_cast<TmPrimary*>(term);
                auto [priority, sub_priority] = _getPriority(tp->op_name);
                std::vector<OutputResult> sub_list;
                for (int i = 0; i < tp->params.size(); ++i) {
                    auto [sub_pri, sub_res] = _arithTerm2String(tp->params[i].get());
                    if (sub_pri < sub_priority[i]) {
                        sub_res.appendLast(")"); sub_res.pushStart("(");
                    }
                    sub_list.push_back(sub_res);
                }
                if (tp->params.size() == 1) {
                    auto res = sub_list[0];
                    if (tp->op_name == "neg") {
                        res.pushStart("-");
                    } else res.pushStart(tp->op_name + " ");
                    return {priority, res};
                } else if (tp->params.size() == 2) {
                    sub_list[0].appendLast(" " + tp->op_name);
                    return {priority, defaultMerge(sub_list[0], sub_list[1])};
                } else LOG(FATAL) << "known arity " << tp->op_name;
            }
            default: return {1e9, _appTerm2String(term)};
        }
    }

    OutputResult _appTerm2String(TermData* term) {
        switch (term->getType()) {
            case TermType::CONS: {
                auto* tc = dynamic_cast<TmCons*>(term);
                auto res = _pathTerm2String(tc->body.get());
                res.pushStart(tc->cons_name + " ");
                return res;
            }
            case TermType::LABEL: {
                auto* tl = dynamic_cast<TmLabel*>(term);
                auto res = _pathTerm2String(tl->body.get());
                res.pushStart("label ");
                return res;
            }
            case TermType::UNLABEL: {
                auto* tu = dynamic_cast<TmUnlabel*>(term);
                auto res = _pathTerm2String(tu->body.get());
                res.pushStart("unlabel ");
                return res;
            }
            case TermType::REWRITE: {
                auto* tr = dynamic_cast<TmRewrite*>(term);
                if (KIsHighlightRewrite) {
                    KIsHighlightRewrite = false;
                    auto res = _pathTerm2String(tr->body.get());
                    KIsHighlightRewrite = true;
                    res.pushStart("<mark>"); res.appendLast("</mark>");
                    return res;
                } else {
                    auto res = _pathTerm2String(tr->body.get());
                    res.pushStart("rewrite ");
                    return res;
                }
            }
            case TermType::APP: {
                auto* ta = dynamic_cast<TmApp*>(term);
                auto func_res = _appTerm2String(ta->func.get());
                auto param_res = _pathTerm2String(ta->param.get());
                return defaultMerge(func_res, param_res);
            }
            default: return _pathTerm2String(term);
        }
    }

    OutputResult _aTerm2String(TermData* term) {
        switch (term->getType()) {
            case TermType::TUPLE: {
                auto* tt = dynamic_cast<TmTuple*>(term);
                std::vector<OutputResult> fields;
                for (auto& field: tt->fields) fields.push_back(_term2String(field.get(), false));
                for (int i = 0; i + 1 < fields.size(); ++i) {
                    fields[i].appendLast(",");
                }
                auto res = fields[0];
                for (int i = 1; i < fields.size(); ++i) res = defaultMerge(res, fields[i]);
                if (res.output_list.size() == 1) {
                    res.pushStart("{"); res.appendLast("}");
                } else {
                    res.addIndent();
                    res = defaultMerge(OutputResult("{"), defaultMerge(res, OutputResult("}")));
                }
                return res;
            }
            case TermType::VALUE: return term->toString();
            case TermType::VAR: return term->toString();
            default: {
                auto res = _term2String(term, false);
                res.pushStart("("); res.appendLast(")");
                return res;
            }
        }
    }

    OutputResult _funcDef2String(TermData* term, const std::string& linker, bool is_case_end) {
        std::string def;
        TermData* current_term = term;
        bool is_anonymous_match = false;
        while (current_term->getType() == TermType::FUNC) {
            auto* tf = dynamic_cast<TmFunc*>(current_term);
            if (tf->name[0] == '_') {
                if (tf->body->getType() != TermType::MATCH) {
                    LOG(FATAL) << "Unexpected definition " << term->toString();
                }
                is_anonymous_match = true;
            } else {
                def += " " + tf->name;
            }
            current_term = tf->body.get();
        }
        def += " " + linker;
        if (is_anonymous_match) {
            def += " function";
            OutputResult match_cases = _matchCase2String(dynamic_cast<TmMatch*>(current_term));
            auto res = concat(def, match_cases);
            return res;
        } else {
            auto body_res = _term2String(current_term, is_case_end);
            auto res = defaultMerge(def, body_res);
            res.addIndent(1);
            return res;
        }
    }
}

std::string io::term2String(syntax::TermData *term, bool is_highlight) {
    return term2OutputResult(term, is_highlight).toString();
}

OutputResult io::term2OutputResult(syntax::TermData *term, bool is_highlight) {
    KIsHighlightRewrite = is_highlight;
    auto res = _term2String(term, is_highlight);
    KIsHighlightRewrite = false;
    return res;
}

OutputResult io::funcDef2OutputResult(syntax::TermData *term, const std::string &linker, bool is_highlight) {
    KIsHighlightRewrite = is_highlight;
    auto res = _funcDef2String(term, linker, false);
    KIsHighlightRewrite = false;
    return res;
}