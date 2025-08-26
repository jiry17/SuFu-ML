//
// Created by pro on 2022/9/21.
//

#include "istool/incre/analysis/incre_instru_types.h"
#include "istool/incre/language/incre_rewriter.h"
#include "glog/logging.h"

using namespace incre::syntax;
using namespace incre::semantics;

VLabeledCompress::VLabeledCompress(const Data &_v, int _id): VCompress(_v), id(_id) {
}
std::string VLabeledCompress::toString() const {
    return "Label[" + std::to_string(id) + "] " + body.toString();
}

TyLabeledCompress::TyLabeledCompress(const syntax::Ty &_ty, int _id): TyCompress(_ty), id(_id) {
}
std::string TyLabeledCompress::toString() const {
    return "Packed[" + std::to_string(id) + "] " + body->toString();
}

TmLabeledLabel::TmLabeledLabel(const syntax::Term &_content, int _id): TmLabel(_content), id(_id) {
}
std::string TmLabeledLabel::toString() const {
    return "label[" + std::to_string(id) + "] " + body->toString();
}

TmLabeledUnlabel::TmLabeledUnlabel(const Term &_content, int _id): TmUnlabel(_content), id(_id) {
}
std::string TmLabeledUnlabel::toString() const {
    return "unlabel[" + std::to_string(id) + "] " + body->toString();
}

TmLabeledRewrite::TmLabeledRewrite(const syntax::Term &_content, int _id): TmRewrite(_content), id(_id) {
}
std::string TmLabeledRewrite::toString() const {
    return "rewrite[" + std::to_string(id) + "] " + body->toString();
}

Ty IncreLabeledTypeRewriter::_rewrite(TyCompress *type, const Ty &_type) {
    auto* labeled_type = dynamic_cast<TyLabeledCompress*>(type);
    if (!labeled_type) LOG(FATAL) << "expect TyLabeledCompress, but got " << type->toString();
    return std::make_shared<TyLabeledCompress>(rewrite(type->body), labeled_type->id);
}

Data IncreLabeledEvaluator::_evaluate(syntax::TmLabel *term, const IncreContext &ctx) {
    auto* labeled_term = dynamic_cast<TmLabeledLabel*>(term);
    if (!labeled_term) LOG(FATAL) << "Expect TmLabeledLabel, but got " << term->toString();
    return Data(std::make_shared<VLabeledCompress>(evaluate(term->body.get(), ctx), labeled_term->id));
}

Ty incre::types::IncreLabeledTypeChecker::_typing(syntax::TmLabel *term, const IncreContext &ctx) {
    auto* labeled_term = dynamic_cast<TmLabeledLabel*>(term);
    if (!labeled_term) LOG(FATAL) << "Expect TmLabeledLabel, but got " << term->toString();
    auto body = typing(term->body.get(), ctx);
    checkAndUpdate(BASE, body.get());
    return std::make_shared<TyLabeledCompress>(body, labeled_term->id);
}

Ty incre::types::IncreLabeledTypeChecker::_typing(syntax::TmUnlabel *term, const IncreContext &ctx) {
    auto* labeled_term = dynamic_cast<TmLabeledUnlabel*>(term);
    if (!labeled_term) LOG(FATAL) << "Expect TmLabeledUnlabel, but got " << term->toString();
    auto content_var = getTmpVar(BASE);
    auto full_type = std::make_shared<TyLabeledCompress>(content_var, labeled_term->id);
    unify(full_type, typing(term->body.get(), ctx));
    return content_var;
}

void incre::types::IncreLabeledTypeChecker::_unify(syntax::TyCompress *x, syntax::TyCompress *y, const syntax::Ty &_x,
                                                   const syntax::Ty &_y) {
    auto* lx = dynamic_cast<TyLabeledCompress*>(x);
    if (!lx) throw IncreTypingError("Expect TyLabeledCompress, but got " + x->toString());
    auto* ly = dynamic_cast<TyLabeledCompress*>(y);
    if (!ly)  throw IncreTypingError("Expect TyLabeledCompress, but got " + y->toString());
    if (lx->id != ly->id) throw IncreTypingError("Incompatible compress label: " + x->toString() + " " + y->toString());
    unify(lx->body, ly->body);
}

namespace {
    class _LabeledTypeVarRewriter: public IncreTypeRewriter {
    public:
        std::unordered_map<int, Ty> replace_map;
        _LabeledTypeVarRewriter(const std::unordered_map<int, Ty> _replace_map): replace_map(_replace_map) {}
    protected:
        Ty _rewrite(TyVar* type, const Ty& _type) override {
            if (type->is_bounded()) {
                return rewrite(type->get_bound_type());
            }
            auto [index, _level, _info] = type->get_var_info();
            auto it = replace_map.find(index);
            if (it == replace_map.end()) return _type;
            return it->second;
        }
        Ty _rewrite(TyCompress* type, const Ty& _type) override {
            auto* labeled_type = dynamic_cast<TyLabeledCompress*>(type);
            if (!labeled_type) throw incre::types::IncreTypingError("Expect LabeledTyCompress, but get " + type->toString());
            return std::make_shared<TyLabeledCompress>(rewrite(type->body), labeled_type->id);
        }
    };

    void _collectRangeMap(TypeData* type, std::unordered_map<int, VarRange>& range_map) {
        if (type->getType() == TypeType::VAR) {
            auto* tv = dynamic_cast<TyVar*>(type);
            if (!tv->is_bounded()) {
                auto [index, _, info] = tv->get_var_info();
                range_map[index] = info;
            }
        }
        for (auto& sub_type: getSubTypes(type)) {
            _collectRangeMap(sub_type.get(), range_map);
        }
    }
}

Ty incre::types::IncreLabeledTypeChecker::instantiate(const Ty &x) {
    if (x->getType() == TypeType::POLY) {
        auto* xp = dynamic_cast<TyPoly*>(x.get());
        std::unordered_map<int, Ty> replace_map;
        std::unordered_map<int, VarRange> range_map;
        _collectRangeMap(x.get(), range_map);
        for (auto index: xp->var_list) {
            auto it = range_map.find(index); assert(it != range_map.end());
            replace_map[index] = getTmpVar(it->second);
        }
        auto* rewriter = new _LabeledTypeVarRewriter(replace_map);
        auto res = rewriter->rewrite(xp->body);
        delete rewriter; return res;
    } else return x;
}