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

#include "codegen/module_builder.hpp"

namespace codegen {

template<typename Type> class variable {
  llvm::Instruction* variable_;
  std::string name_;

public:
  static_assert(!std::is_const_v<Type>);
  static_assert(!std::is_volatile_v<Type>);

  explicit variable(std::string const& n) : name_(n) {
    auto& mb = *detail::current_builder;

    auto alloca_builder = llvm::IRBuilder<>(&mb.function_->getEntryBlock(), mb.function_->getEntryBlock().begin());
    variable_ = alloca_builder.CreateAlloca(detail::type<Type>::llvm(), nullptr, name_);

    auto line_no = mb.source_code_.add_line(fmt::format("{} {};", detail::type<Type>::name(), name_));
    auto dbg_variable =
        mb.dbg_builder_.createAutoVariable(mb.dbg_scope_, name_, mb.dbg_file_, line_no, detail::type<Type>::dbg());
    //mb.dbg_builder_.insertDeclare(variable_, dbg_variable, mb.dbg_builder_.createExpression(),
    //                              llvm::DebugLoc::get(line_no, 1, mb.dbg_scope_), mb.ir_builder_.GetInsertBlock());
  }

  template<typename Value> explicit variable(std::string const& n, Value const& v) : variable(n) { set<Value>(v); }

  variable(variable const&) = delete;
  variable(variable&&) = delete;

  value<Type> get() const {
    auto v = detail::current_builder->ir_builder_.CreateAlignedLoad(variable_, detail::type<Type>::alignment);
    return value<Type>{v, name_};
  }

  template<typename Value> void set(Value const& v) {
    static_assert(std::is_same_v<Type, typename Value::value_type>);
    auto& mb = *detail::current_builder;
    auto line_no = mb.source_code_.add_line(fmt::format("{} = {};", name_, v));
    //mb.ir_builder_.SetCurrentDebugLocation(llvm::DebugLoc::get(line_no, 1, mb.dbg_scope_));
    mb.ir_builder_.CreateAlignedStore(v.eval(), variable_, detail::type<Type>::alignment);
  }

  template<typename T = Type, typename Value>
  typename std::enable_if_t<std::is_array_v<T>, void> set(Value const& v) {


  }
  

  // TODO
  // address-of operator gets you the pointer to the variable.
  //Type *operator&() { }

  template<typename T = Type, typename Value>
  typename std::enable_if_t<std::is_array_v<T> &&
                            std::is_integral_v<typename Value::value_type>,
	    value<std::remove_all_extents_t<T>>>
  operator[](Value const& v) & {
    using ElementType = typename std::remove_all_extents_t<T>;

    auto& mb = *detail::current_builder;

    auto idx = v.eval();

    if constexpr (sizeof(typename Value::value_type) < sizeof(uint64_t)) {
      if constexpr (std::is_unsigned_v<Value::value_type>) {
        idx = mb.ir_builder_.CreateZExt(idx, detail::type<uint64_t>::llvm());
      } else {
        idx = mb.ir_builder_.CreateSExt(idx, detail::type<int64_t>::llvm());
      }
    }

    auto elem_ptr = mb.ir_builder_.CreateInBoundsGEP(variable_, idx);

    auto temp_storage = mb.ir_builder_.CreateAlloca(detail::type<ElementType>::llvm(), nullptr, "temporary_storage");

    auto load = mb.ir_builder_.CreateAlignedLoad(elem_ptr, detail::type<ElementType>::alignment);

    return value<std::remove_all_extents_t<T>>{load, "test_name"};
  }

  template<typename T = Type, typename IndexValue, typename Value>
  typename std::enable_if_t<std::is_array_v<T> &&
                              std::is_integral_v<typename IndexValue::value_type> &&
			      std::is_same_v<std::remove_all_extents_t<T>, typename Value::value_type>,
	                    value<std::remove_all_extents_t<T>>>
  setElem(IndexValue const& idx_v, Value const&& value_v) {
    auto& mb = *detail::current_builder;
    auto idx = idx_v.eval();

    if constexpr (sizeof(typename IndexValue::value_type) < sizeof(uint64_t)) {
      if constexpr (std::is_unsigned_v<IndexValue::value_type>) {
        idx = mb.ir_builder_.CreateZExt(idx, detail::type<uint64_t>::llvm());
      } else {
        idx = mb.ir_builder_.CreateSExt(idx, detail::type<int64_t>::llvm());
      }
    }

    auto elem = mb.ir_builder_.CreateInBoundsGEP(variable_, idx);
    mb.ir_builder_.CreateAlignedStore(value_v.eval(), idx, detail::type<typename Value::value_type>::alignment);
    return std::move(value_v);
  }

  // lvalue
  /*
  template<typename T = Type, typename Value>
  typename std::enable_if_t<std::is_array_v<T> &&
                            std::is_integral_v<typename Value::value_type>,
	    value<std::remove_all_extents_t<T>>>
  operator[](Value const& v) && {
    //static_assert(sizeof(Value::value_type) < sizeof(int64_t));
    auto& mb = *detail::current_builder;

    auto idx = v.eval();

    if constexpr (sizeof(typename Value::value_type) < sizeof(uint64_t)) {
      if constexpr (std::is_unsigned_v<Value::value_type>) {
        idx = mb.ir_builder_.CreateZExt(idx, detail::type<uint64_t>::llvm());
      } else {
        idx = mb.ir_builder_.CreateSExt(idx, detail::type<int64_t>::llvm());
      }
    }

    auto elem = mb.ir_builder_.CreateInBoundsGEP(variable_, idx);
    std::string value_name = fmt::format("{}[{}]", name_, idx.name_);
    return value<std::remove_all_extents_t<T>>{elem, value_name};
  }
  */


};

} // namespace codegen
