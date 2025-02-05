/*
 * Copyright © 2019 Paweł Dziepak
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "module_builder.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace codegen {

template<ConditionType Condition, typename TrueBlock, typename FalseBlock,
         typename = std::enable_if_t<std::is_same_v<typename std::decay_t<Condition>::value_type, bool>>>
inline void if_(Condition&& cnd, TrueBlock&& tb, FalseBlock&& fb) {
  auto& mb = *module_builder::current_builder();

  auto line_no = mb.source_code_.add_line(fmt::format("if ({}) {{", cnd));
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));

  auto true_block = llvm::BasicBlock::Create(mb.context(), "true_block", mb.current_function());
  auto false_block = llvm::BasicBlock::Create(mb.context(), "false_block");
  auto merge_block = llvm::BasicBlock::Create(mb.context(), "merge_block");

  mb.ir_builder().CreateCondBr(cnd.eval(), true_block, false_block);

  mb.ir_builder().SetInsertPoint(true_block);
  mb.source_code_.enter_scope();

  assert(!mb.exited_block_);
  tb();
  mb.source_code_.leave_scope();

  line_no = mb.source_code_.add_line("} else {");

  if (!mb.exited_block_) {
    mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
    mb.ir_builder().CreateBr(merge_block);
  }
  mb.exited_block_ = false;

  mb.current_function()->getBasicBlockList().push_back(false_block);
  mb.ir_builder().SetInsertPoint(false_block);

  mb.source_code_.enter_scope();
  fb();
  mb.source_code_.leave_scope();

  line_no = mb.source_code_.add_line("}");

  if (!mb.exited_block_) {
    mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
    mb.ir_builder().CreateBr(merge_block);
  }
  mb.exited_block_ = false;

  mb.current_function()->getBasicBlockList().push_back(merge_block);
  mb.ir_builder().SetInsertPoint(merge_block);
}

template<typename Condition, typename TrueBlock,
         typename = std::enable_if_t<std::is_same_v<typename std::decay_t<Condition>::value_type, bool>>>
inline void if_(Condition&& cnd, TrueBlock&& tb) {
  auto& mb = *module_builder::current_builder();

  auto line_no = mb.source_code_.add_line(fmt::format("if ({}) {{", cnd));
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));

  auto true_block = llvm::BasicBlock::Create(mb.context(), "true_block", mb.current_function());
  auto merge_block = llvm::BasicBlock::Create(mb.context(), "merge_block");

  mb.ir_builder().CreateCondBr(cnd.eval(), true_block, merge_block);

  mb.ir_builder().SetInsertPoint(true_block);

  mb.source_code_.enter_scope();
  assert(!mb.exited_block_);
  tb();
  mb.source_code_.leave_scope();

  line_no = mb.source_code_.add_line("}");

  if (!mb.exited_block_) {
    mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
    mb.ir_builder().CreateBr(merge_block);
  }
  mb.exited_block_ = false;

  mb.current_function()->getBasicBlockList().push_back(merge_block);
  mb.ir_builder().SetInsertPoint(merge_block);
}

template<typename ReturnType, typename... Arguments, typename... Values>
inline value<ReturnType> call(function_ref<ReturnType, Arguments...> const& fn, Values&&... args) {
  static_assert((std::is_same_v<Arguments, typename std::decay_t<Values>::value_type> && ...));

  auto& mb = *module_builder::current_builder();

  {
    auto str = std::stringstream{};
    str << fn.name() << "_ret = " << fn.name() << "(";
    (void)(str << ... << fmt::format("{}, ", args));
    str << ");";
    auto line_no = mb.source_code_.add_line(str.str());
    mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  }

  auto values = std::vector<llvm::Value*>{};
  [[maybe_unused]] auto _ = {0, ((values.emplace_back(args.eval())), 0)...};

  auto ret = mb.ir_builder().CreateCall(fn, values);
  return value<ReturnType>{ret, fmt::format("{}_ret", fn.name())};
}

template<typename Pointer, typename = std::enable_if_t<std::is_pointer_v<typename std::decay_t<Pointer>::value_type>>>
inline auto load(Pointer ptr) {
  using value_type = std::remove_cv_t<std::remove_pointer_t<typename std::decay_t<Pointer>::value_type>>;
  auto& mb = *module_builder::current_builder();

  auto id = fmt::format("val{}", detail::id_counter++);

  auto line_no = mb.source_code_.add_line(fmt::format("{} = *{}", id, ptr));
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  auto v = mb.ir_builder().CreateAlignedLoad(ptr.eval(), llvm::MaybeAlign(detail::type<value_type>::alignment));

  auto dbg_value = mb.debug_builder().createAutoVariable(
      mb.source_code_.debug_scope(), id, mb.source_code_.debug_file(), line_no, detail::type<value_type>::dbg());
  mb.debug_builder().insertDbgValueIntrinsic(v, dbg_value, mb.debug_builder().createExpression(),
                                             mb.get_debug_location(line_no), mb.ir_builder().GetInsertBlock());

  return value<value_type>{v, id};
}

template<
    typename Value, typename Pointer,
    typename = std::enable_if_t<std::is_pointer_v<typename std::decay_t<Pointer>::value_type> &&
                                !std::is_const_v<std::remove_pointer_t<typename std::decay_t<Pointer>::value_type>> &&
                                std::is_same_v<typename std::decay_t<Value>::value_type,
                                               std::remove_pointer_t<typename std::decay_t<Pointer>::value_type>>>>
inline void store(Value v, Pointer ptr) {
  using value_type = std::remove_pointer_t<typename std::decay_t<Pointer>::value_type>;
  auto& mb = *module_builder::current_builder();

  auto line_no = mb.source_code_.add_line(fmt::format("*{} = {}", ptr, v));
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  mb.ir_builder().CreateAlignedStore(v.eval(), ptr.eval(), llvm::MaybeAlign(detail::type<value_type>::alignment));
}

template<typename ConditionFn, typename Body,
         typename = std::enable_if_t<std::is_same_v<typename std::invoke_result_t<ConditionFn>::value_type, bool>>>
inline void while_(ConditionFn cnd_fn, Body bdy) {
  auto& mb = *module_builder::current_builder();

  auto line_no = mb.source_code_.current_line() + 1;
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  auto cnd = cnd_fn();
  mb.source_code_.add_line(fmt::format("while ({}) {{", cnd));

  auto while_continue = llvm::BasicBlock::Create(mb.context(), "while_continue", mb.current_function());
  auto while_iteration = llvm::BasicBlock::Create(mb.context(), "while_iteration");
  auto while_break = llvm::BasicBlock::Create(mb.context(), "while_break");

  auto parent_loop = std::exchange(mb.current_loop_, module_builder::loop{while_continue, while_break});

  mb.ir_builder().CreateBr(while_continue);
  mb.ir_builder().SetInsertPoint(while_continue);

  mb.ir_builder().CreateCondBr(cnd_fn().eval(), while_iteration, while_break);

  mb.source_code_.enter_scope();

  mb.current_function()->getBasicBlockList().push_back(while_iteration);
  mb.ir_builder().SetInsertPoint(while_iteration);

  assert(!mb.exited_block_);
  bdy();

  mb.source_code_.leave_scope();

  line_no = mb.source_code_.add_line("}");

  if (!mb.exited_block_) {
    mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
    mb.ir_builder().CreateBr(while_continue);
  }
  mb.exited_block_ = false;

  mb.current_function()->getBasicBlockList().push_back(while_break);
  mb.ir_builder().SetInsertPoint(while_break);

  mb.current_loop_ = parent_loop;
}

inline void break_() {
  auto& mb = *module_builder::current_builder();
  assert(mb.current_loop_.break_block_);

  mb.exited_block_ = true;

  auto line_no = mb.source_code_.add_line("break;");
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));

  mb.ir_builder().CreateBr(mb.current_loop_.break_block_);
}

inline void continue_() {
  auto& mb = *module_builder::current_builder();
  assert(mb.current_loop_.continue_block_);

  mb.exited_block_ = true;

  auto line_no = mb.source_code_.add_line("continue;");
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));

  mb.ir_builder().CreateBr(mb.current_loop_.continue_block_);
}

inline value<bool> true_() {
  return constant(true);
}

inline value<bool> false_() {
  return constant(false);
}

inline void return_() {
  auto& mb = *module_builder::current_builder();
  auto line_no = mb.source_code_.add_line("return;");
  mb.exited_block_ = true;
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  mb.ir_builder().CreateRetVoid();
}

template<typename Value> inline void return_(Value v) {
  auto& mb = *module_builder::current_builder();
  mb.exited_block_ = true;
  auto line_no = mb.source_code_.add_line(fmt::format("return {};", v));
  mb.ir_builder().SetCurrentDebugLocation(mb.get_debug_location(line_no));
  mb.ir_builder().CreateRet(v.eval());
}

} // namespace codegen
