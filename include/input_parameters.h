// --------------------------------------------------------------------------
//
// Copyright (C) 2018 by the ExWave authors
//
// This file is part of the ExWave library.
//
// The ExWave library is free software; you can use it, redistribute it,
// and/or modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version. The full text of the
// license can be found in the file LICENSE at the top level of the ExWave
// distribution.
//
// --------------------------------------------------------------------------

#ifndef input_parameters_h_
#define input_parameters_h_

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/manifold_lib.h>
#include "parameters.h"
#include "utilities.h"

namespace HDG_WE
{
  using namespace dealii;

  extern double grid_transform_factor;

  // create your materials in the vector mats and assign the material values (must conform with the material_ids you set later)
  std::vector<Material> input_materials();

  // set grid transform factor
  void set_grid_transform_factor(double in);

  // get grid transform factor
  double get_grid_transform_factor();

  // if you want to modify a cartesian grid by a function, fill in the formula here
  template <int dim>
  Point<dim> grid_transform (const Point<dim> &in)
  {
    Point<dim> out = in;
    double sin_val = get_grid_transform_factor();
    for (unsigned int d=0; d<dim; ++d)
      sin_val *= std::sin(numbers::PI*in[d]);
    out[0] = in[0] + sin_val;
    return out;
  }


  // create your geometry
  // after creating the geometry, set boundary ids! they define the type of boundary
  // conditions
  // 1 - soft wall - normal velocity component is zero
  // 2 - hard wall - pressure is zero
  // 3 - absorbing wall - mimics an open domain by the first order absorbing condition
  template <int dim>
  void input_geometry_description(Triangulation<dim> &tria, const Parameters &parameters)
  {
    switch (parameters.initial_cases)
      {
      case 1:
      {
        GridGenerator::subdivided_hyper_cube(tria,parameters.n_initial_intervals,0,1);
        set_grid_transform_factor(parameters.grid_transform_factor);
        GridTools::transform (&grid_transform<dim>, tria);

        // set your boundary conditions!
        typename Triangulation<dim>::active_cell_iterator cell = tria.begin_active(),endc = tria.end();
        for (; cell!=endc; ++cell)
          for (unsigned f=0; f<GeometryInfo<dim>::faces_per_cell; ++f)
            if (cell->face(f)->at_boundary())
              cell->face(f)->set_boundary_id(parameters.boundary_id);

        // set the materials according to your material definitions above!
        cell = tria.begin_active();
        for (; cell!=endc; ++cell)
          cell->set_material_id(0);

        break;
      }
      case 2:
      {
        Triangulation<dim> tria1, tria2;
        GridGenerator::hyper_shell(tria1, Point<dim>(), 0.4, std::sqrt(3), 6);
        GridGenerator::hyper_ball(tria2, Point<dim>(), 0.4);
        GridGenerator::merge_triangulations(tria1, tria2, tria);
        tria.set_all_manifold_ids(0);
        for (typename Triangulation<dim>::cell_iterator cell = tria.begin();
             cell != tria.end(); ++cell)
          {
            for (unsigned int f=0; f<GeometryInfo<dim>::faces_per_cell; ++f)
              {
                bool face_at_sphere_boundary = true;
                for (unsigned int v=0; v<GeometryInfo<dim-1>::vertices_per_cell; ++v)
                  if (std::abs(cell->face(f)->vertex(v).norm()-0.4) > 1e-12)
                    face_at_sphere_boundary = false;
                if (face_at_sphere_boundary)
                  cell->face(f)->set_all_manifold_ids(1);
              }
            if (cell->center().norm() > 0.4)
              cell->set_material_id(1);
            else
              cell->set_material_id(0);
          }
        const SphericalManifold<dim> spherical_manifold;
        tria.set_manifold(1, spherical_manifold);
        TransfiniteInterpolationManifold<dim> transfinite_manifold;
        transfinite_manifold.initialize(tria);
        tria.set_manifold(0, transfinite_manifold);

        typename Triangulation<dim>::active_cell_iterator cell = tria.begin_active(),endc = tria.end();
        for (; cell!=endc; ++cell)
          for (unsigned f=0; f<GeometryInfo<dim>::faces_per_cell; ++f)
            if (cell->face(f)->at_boundary())
              cell->face(f)->set_boundary_id(1);

        break;
      }
      default:
        Assert(false,ExcNotImplemented());
      }
    tria.refine_global(parameters.n_refinements);
    tria.set_manifold(0, FlatManifold<dim>());
  }



  // define the "exact solution" (first dim components are for velocity, last for the pressure)
  // this function is also used for the initial conditions
  template <int dim>
  double input_exact_solution(const Point<dim> &p, const double t, const unsigned int component,
                              const bool time_derivative, const int initial_cases, const int membrane_modes)
  {
    double return_value = 0.0;
    switch (initial_cases)
      {
      case 1:
      {
        if (time_derivative)
          {
            if (component == dim)
              return_value = -membrane_modes*std::sqrt(dim)*numbers::PI*std::sin(membrane_modes*std::sqrt(dim)*numbers::PI*t);
            else
              return_value = -membrane_modes*numbers::PI*std::cos(membrane_modes*std::sqrt(dim)*numbers::PI*t);
          }
        else
          {
            if (component == dim)
              return_value = std::cos(membrane_modes*std::sqrt(dim)*numbers::PI*t);
            else
              return_value = -std::sin(membrane_modes*std::sqrt(dim)*numbers::PI*t)/std::sqrt(dim);
          }
        for (unsigned int d=0; d<dim; ++d)
          if (d != component)
            return_value *= std::sin(membrane_modes*numbers::PI*p(d));
          else
            return_value *= std::cos(membrane_modes*numbers::PI*p(d));
        break;
      }
      case 2:
      {
        const double fact = 500.0;
        if(component == dim)
          return_value = std::sqrt(fact*fact*fact/(8*numbers::PI*numbers::PI*numbers::PI)) *
                         std::exp(-fact*((p[0]-0.6)*(p[0]-0.6)+(p[1]-0.6)*(p[1]-0.6)+(p[2]-0.6)*(p[2]-0.6)));
        break;
      }
      default:
        Assert(false,ExcNotImplemented());
      }
    return return_value;
  }
}

#endif
