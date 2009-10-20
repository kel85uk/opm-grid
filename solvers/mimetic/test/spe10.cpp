//===========================================================================
//
// File: mimetic_periodic_test.cpp
//
// Created: Wed Aug 19 13:42:04 2009
//
// Author(s): B�rd Skaflestad     <bard.skaflestad@sintef.no>
//            Atgeirr F Rasmussen <atgeirr@sintef.no>
//
// $Date$
//
// $Revision$
//
//===========================================================================

/*
  Copyright 2009 SINTEF ICT, Applied Mathematics.
  Copyright 2009 Statoil ASA.

  This file is part of The Open Reservoir Simulator Project (OpenRS).

  OpenRS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OpenRS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OpenRS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <fstream>

#include <boost/array.hpp>

#include <dune/common/Units.hpp>
#include <dune/common/param/ParameterGroup.hpp>

#include <dune/grid/io/file/vtk/vtkwriter.hh>

#include <dune/grid/CpGrid.hpp>

#include <dune/solvers/common/PeriodicHelpers.hpp>
#include <dune/solvers/common/BoundaryConditions.hpp>
#include <dune/solvers/common/GridInterfaceEuler.hpp>
#include <dune/solvers/common/ReservoirPropertyCapillary.hpp>
#include <dune/solvers/common/SimulatorUtilities.hpp>
#include <dune/solvers/common/Matrix.hpp>

#include <dune/solvers/mimetic/MimeticIPEvaluator.hpp>
#include <dune/solvers/mimetic/IncompFlowSolverHybrid.hpp>

using namespace Dune;

void fill_perm(std::vector<double>& k)
{
    std::ifstream permdata("spe_perm.dat");

    k.clear(); k.reserve(3 * 60 * 220 * 85);
    double perm;
    while (permdata >> perm) {
        k.push_back(unit::convert::from(perm, prefix::milli*unit::darcy));
    }
}


int main(int argc, char** argv)
{
    typedef Dune::GridInterfaceEuler<CpGrid>                       GI;
    typedef GI  ::CellIterator                                     CI;
    typedef CI  ::FaceIterator                                     FI;
    typedef Dune::BoundaryConditions<true, false>                  BCs;
    typedef Dune::ReservoirPropertyCapillary<3>                    RI;
    typedef Dune::IncompFlowSolverHybrid<GI, RI, BCs,
        Dune::MimeticIPEvaluator> FlowSolver;

    parameter::ParameterGroup param(argc, argv);
    CpGrid grid;
    grid.init(param);
    //grid.setUniqueBoundaryIds(true);
    GridInterfaceEuler<CpGrid> g(grid);

    typedef Dune::FlowBC BC;
    BCs flow_bc(7);
    flow_bc.flowCond(5) = BC(BC::Dirichlet, 100*Dune::unit::barsa);
    //flow_bc.flowCond(6) = BC(BC::Dirichlet, 0.00*Dune::unit::barsa);

    RI r;
    r.init(g.numberOfCells());

    std::vector<double> permdata;
    fill_perm(permdata);
    ASSERT (int(permdata.size()) == 3 * 60 * 220 * 85);
    Dune::SharedFortranMatrix Perm(60 * 220 * 85, 3, &permdata[0]);
    const int imin = param.get<int>("imin");
    const int jmin = param.get<int>("jmin");
    const int kmin = param.get<int>("kmin");
    for (int c = 0; c < g.numberOfCells(); ++c) {
        boost::array<int,3> ijk;
        grid.getIJK(c, ijk);
        ijk[0] += imin;  ijk[1] += jmin;  ijk[2] += kmin;
        const int gc = ijk[0] + 60*(ijk[1] + 220*ijk[2]);
        
        RI::SharedPermTensor K = r.permeabilityModifiable(c);
        K(0,0) = Perm(gc,0);
        K(1,1) = Perm(gc,1);
        K(2,2) = Perm(gc,2);
    }


    FlowSolver solver;
    solver.init(g, r, flow_bc);

#if 1
    std::vector<double> src(g.numberOfCells(), 0.0);
    std::vector<double> sat(g.numberOfCells(), 0.0);

    CI::Vector gravity;
    gravity[0] = gravity[1] = gravity[2] = 0.0;
#if 1
    gravity[2] = Dune::unit::gravity;
#endif

    solver.solve(r, sat, flow_bc, src, gravity, 5.0e-6, 3);
#endif

#if 0
    std::vector<double> cell_velocity;
    estimateCellVelocity(cell_velocity, g, solver.getSolution());
    std::vector<double> cell_pressure;
    getCellPressure(cell_pressure, g, solver.getSolution());

    Dune::VTKWriter<CpGrid::LeafGridView> vtkwriter(grid.leafView());
    vtkwriter.addCellData(cell_velocity, "velocity");
    vtkwriter.addCellData(cell_pressure, "pressure");
    vtkwriter.write("spe10_test_output", Dune::VTKOptions::ascii);
#endif
    return 0;
}
