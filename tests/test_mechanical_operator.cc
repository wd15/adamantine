/* Copyright (c) 2022, the adamantine authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/lac/sparsity_pattern.h>
#define BOOST_TEST_MODULE MechanicalOperator

#include <Geometry.hh>
#include <MechanicalOperator.hh>

#include <deal.II/base/quadrature_lib.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_nothing.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/numerics/vector_tools.h>

#include <weak_forms/weak_forms.h>

#include <boost/property_tree/ptree.hpp>

#include "main.cc"

template <int dim>
void right_hand_side(const std::vector<dealii::Point<dim>> &points,
                     std::vector<dealii::Tensor<1, dim>> &values)
{
  dealii::Point<dim> point_1, point_2;
  point_1(0) = 0.5;
  point_2(0) = -0.5;

  for (unsigned int point_n = 0; point_n < points.size(); ++point_n)
  {
    if (((points[point_n] - point_1).norm_square() < 0.2 * 0.2) ||
        ((points[point_n] - point_2).norm_square() < 0.2 * 0.2))
      values[point_n][0] = 1.0;
    else
      values[point_n][0] = 0.0;

    if (points[point_n].norm_square() < 0.2 * 0.2)
      values[point_n][1] = 1.0;
    else
      values[point_n][1] = 0.0;
  }
}

BOOST_AUTO_TEST_CASE(elastostatic)
{
  MPI_Comm communicator = MPI_COMM_WORLD;
  int constexpr dim = 3;

  // Create the Geometry
  boost::property_tree::ptree geometry_database;
  geometry_database.put("import_mesh", false);
  geometry_database.put("length", 6);
  geometry_database.put("length_divisions", 3);
  geometry_database.put("height", 6);
  geometry_database.put("height_divisions", 3);
  geometry_database.put("width", 6);
  geometry_database.put("width_divisions", 3);
  adamantine::Geometry<dim> geometry(communicator, geometry_database);
  // Create the DoFHandler
  dealii::hp::FECollection<dim> fe_collection;
  fe_collection.push_back(dealii::FESystem<dim>(dealii::FE_Q<dim>(2) ^ dim));
  fe_collection.push_back(
      dealii::FESystem<dim>(dealii::FE_Nothing<dim>() ^ dim));
  dealii::DoFHandler<dim> dof_handler(geometry.get_triangulation());
  dof_handler.distribute_dofs(fe_collection);
  dealii::AffineConstraints<double> affine_constraints;
  dealii::DoFTools::make_hanging_node_constraints(dof_handler,
                                                  affine_constraints);
  dealii::VectorTools::interpolate_boundary_values(
      dof_handler, 0, dealii::Functions::ZeroFunction<dim>(dim),
      affine_constraints);
  affine_constraints.close();
  dealii::hp::QCollection<dim> q_collection;
  q_collection.push_back(dealii::QGauss<dim>(3));
  q_collection.push_back(dealii::QGauss<dim>(1));
  // Create the mechanical database
  boost::property_tree::ptree mechanical_database;
  double const lame_first = 2.;
  double const lame_second = 3.;
  mechanical_database.put("lame_first_param", lame_first);
  mechanical_database.put("lame_second_param", lame_second);

  adamantine::MechanicalOperator<dim> mechanical_operator(communicator,
                                                          mechanical_database);
  mechanical_operator.reinit(dof_handler, affine_constraints, q_collection);

  // deal.II reference implementation
  dealii::SparsityPattern sparsity_pattern;
  dealii::SparseMatrix<double> system_matrix;
  {
    dealii::DoFHandler<dim> ref_dof_handler(geometry.get_triangulation());
    dealii::FESystem<dim> fe(dealii::FE_Q<dim>(2), dim);
    ref_dof_handler.distribute_dofs(fe);

    dealii::Vector<double> system_rhs(ref_dof_handler.n_dofs());

    dealii::AffineConstraints<double> constraints;
    dealii::DoFTools::make_hanging_node_constraints(ref_dof_handler,
                                                    constraints);
    dealii::VectorTools::interpolate_boundary_values(
        ref_dof_handler, 0, dealii::Functions::ZeroFunction<dim>(dim),
        constraints);
    constraints.close();

    dealii::DynamicSparsityPattern dsp(ref_dof_handler.n_dofs(),
                                       ref_dof_handler.n_dofs());
    dealii::DoFTools::make_sparsity_pattern(ref_dof_handler, dsp, constraints,
                                            /*keep_constrained_dofs = */ false);
    sparsity_pattern.copy_from(dsp);

    system_matrix.reinit(sparsity_pattern);

    dealii::QGauss<dim> quadrature_formula(3);
    dealii::FEValues<dim> fe_values(
        fe, quadrature_formula,
        dealii::update_values | dealii::update_gradients |
            dealii::update_quadrature_points | dealii::update_JxW_values);

    unsigned int const dofs_per_cell = fe.n_dofs_per_cell();
    unsigned int const n_q_points = quadrature_formula.size();

    dealii::FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
    dealii::Vector<double> cell_rhs(dofs_per_cell);

    std::vector<dealii::types::global_dof_index> local_dof_indices(
        dofs_per_cell);

    std::vector<double> lambda_values(n_q_points);
    std::vector<double> mu_values(n_q_points);

    dealii::Functions::ConstantFunction<dim> lambda(lame_first),
        mu(lame_second);

    std::vector<dealii::Tensor<1, dim>> rhs_values(n_q_points);

    for (const auto &cell : ref_dof_handler.active_cell_iterators())
    {
      cell_matrix = 0;
      cell_rhs = 0;

      fe_values.reinit(cell);

      lambda.value_list(fe_values.get_quadrature_points(), lambda_values);
      mu.value_list(fe_values.get_quadrature_points(), mu_values);
      right_hand_side(fe_values.get_quadrature_points(), rhs_values);

      for (const unsigned int i : fe_values.dof_indices())
      {
        const unsigned int component_i = fe.system_to_component_index(i).first;

        for (const unsigned int j : fe_values.dof_indices())
        {
          const unsigned int component_j =
              fe.system_to_component_index(j).first;

          for (const unsigned int q_point :
               fe_values.quadrature_point_indices())
          {
            cell_matrix(i, j) +=
                ((fe_values.shape_grad(i, q_point)[component_i] *
                  fe_values.shape_grad(j, q_point)[component_j] *
                  lambda_values[q_point]) +
                 (fe_values.shape_grad(i, q_point)[component_j] *
                  fe_values.shape_grad(j, q_point)[component_i] *
                  mu_values[q_point]) +
                 ((component_i == component_j)
                      ? (fe_values.shape_grad(i, q_point) *
                         fe_values.shape_grad(j, q_point) * mu_values[q_point])
                      : 0)) *
                fe_values.JxW(q_point);
          }
        }
      }

      for (const unsigned int i : fe_values.dof_indices())
      {
        const unsigned int component_i = fe.system_to_component_index(i).first;

        for (const unsigned int q_point : fe_values.quadrature_point_indices())
          cell_rhs(i) += fe_values.shape_value(i, q_point) *
                         rhs_values[q_point][component_i] *
                         fe_values.JxW(q_point);
      }

      cell->get_dof_indices(local_dof_indices);
      constraints.distribute_local_to_global(cell_matrix, local_dof_indices,
                                             system_matrix);
    }
  }
  // Compare the results
  double const tolerance = 1e-12;
  unsigned int const n_dofs = system_matrix.m();
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> src(
      n_dofs);
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> dst_1(
      n_dofs);
  dealii::LA::distributed::Vector<double, dealii::MemorySpace::Host> dst_2(
      n_dofs);
  for (unsigned int i = 0; i < n_dofs; ++i)
  {
    src = 0.;
    src[i] = 1;
    mechanical_operator.vmult(dst_1, src);
    system_matrix.vmult(dst_2, src);
    for (unsigned int j = 0; j < n_dofs; ++j)
      BOOST_CHECK_SMALL(dst_1[j] - dst_2[j], tolerance);
  }
}
