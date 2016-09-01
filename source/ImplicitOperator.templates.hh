/* Copyright (c) 2016, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#ifndef _IMPLICIT_OPERATOR_TEMPLATES_HH_
#define _IMPLICIT_OPERATOR_TEMPLATES_HH_

#include "ImplicitOperator.hh"
#include "utils.hh"

namespace adamantine
{
template <typename NumberType>
ImplicitOperator<NumberType>::ImplicitOperator(
    std::shared_ptr<Operator<NumberType>> explicit_operator, bool jfnk)
    : _jfnk(jfnk), _explicit_operator(explicit_operator)
{
}

template <typename NumberType>
void ImplicitOperator<NumberType>::vmult(
    dealii::LA::distributed::Vector<NumberType> &dst,
    dealii::LA::distributed::Vector<NumberType> const &src) const
{
  if (_jfnk == true)
  {
    dealii::LA::distributed::Vector<NumberType> tmp_dst(dst.get_partitioner());
    dealii::LA::distributed::Vector<NumberType> tmp_src(src);
    tmp_src *= (1. + 1e-10);
    vmult(dst, tmp_src);
    vmult(tmp_dst, src);
    dst -= tmp_dst;
    dst /= 1e-10;
  }
  else
    _explicit_operator->jacobian_vmult(dst, src);

  dst.scale(*_inverse_mass_matrix);
  dst *= -_tau;
  dst += src;
}

template <typename NumberType>
void ImplicitOperator<NumberType>::Tvmult(
    dealii::LA::distributed::Vector<NumberType> & /*dst*/,
    dealii::LA::distributed::Vector<NumberType> const & /*src*/) const
{
  ASSERT_THROW(false, "Function not implemented.");
}

template <typename NumberType>
void ImplicitOperator<NumberType>::vmult_add(
    dealii::LA::distributed::Vector<NumberType> & /*dst*/,
    dealii::LA::distributed::Vector<NumberType> const & /*src*/) const
{
  ASSERT_THROW(false, "Function not implemented.");
}

template <typename NumberType>
void ImplicitOperator<NumberType>::Tvmult_add(
    dealii::LA::distributed::Vector<NumberType> & /*dst*/,
    dealii::LA::distributed::Vector<NumberType> const & /*src*/) const
{
  ASSERT_THROW(false, "Function not implemented.");
}
}

#endif
