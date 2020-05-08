/* Copyright (c) 2016 - 2019, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#include <ImplicitOperator.hh>
#include <instantiation.hh>
#include <utils.hh>

namespace adamantine
{
ImplicitOperator::ImplicitOperator(std::shared_ptr<Operator> explicit_operator,
                                   bool jfnk)
    : _jfnk(jfnk), _explicit_operator(explicit_operator)
{
}

void ImplicitOperator::vmult(
    dealii::LA::distributed::Vector<double> &dst,
    dealii::LA::distributed::Vector<double> const &src) const
{
  if (_jfnk == true)
  {
    dealii::LA::distributed::Vector<double> tmp_dst(dst.get_partitioner());
    dealii::LA::distributed::Vector<double> tmp_src(src);
    tmp_src *= (1. + 1e-10);
    _explicit_operator->vmult(dst, tmp_src);
    _explicit_operator->vmult(tmp_dst, src);
    dst -= tmp_dst;
    dst /= 1e-10;
  }
  else
    _explicit_operator->jacobian_vmult(dst, src);

  dst.scale(*_inverse_mass_matrix);
  dst *= -_tau;
  dst += src;
}

void ImplicitOperator::Tvmult(
    dealii::LA::distributed::Vector<double> & /*dst*/,
    dealii::LA::distributed::Vector<double> const & /*src*/) const
{
  ASSERT_THROW_NOT_IMPLEMENTED();
}

void ImplicitOperator::vmult_add(
    dealii::LA::distributed::Vector<double> & /*dst*/,
    dealii::LA::distributed::Vector<double> const & /*src*/) const
{
  ASSERT_THROW_NOT_IMPLEMENTED();
}

void ImplicitOperator::Tvmult_add(
    dealii::LA::distributed::Vector<double> & /*dst*/,
    dealii::LA::distributed::Vector<double> const & /*src*/) const
{
  ASSERT_THROW_NOT_IMPLEMENTED();
}
} // namespace adamantine
