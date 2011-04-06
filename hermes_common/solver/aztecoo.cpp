// This file is part of Hermes3D
//
// Copyright (c) 2009 hp-FEM group at the University of Nevada, Reno (UNR).
// Email: hpfem-group@unr.edu, home page: http://hpfem.org/.
//
// Hermes3D is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// Hermes3D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Hermes3D; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "aztecoo.h"
#include "../callstack.h"
#ifdef HAVE_KOMPLEX
  #include <Komplex_LinearProblem.h>
#endif

// AztecOO solver //////////////////////////////////////////////////////////////////////////////////

template<typename Scalar>
AztecOOSolver<Scalar>::AztecOOSolver(EpetraMatrix<Scalar> *m, EpetraVector<Scalar> *rhs)
  : IterSolver<Scalar>(), m(m), rhs(rhs)
{
  _F_
#ifdef HAVE_AZTECOO
  #ifndef HAVE_TEUCHOS
    pc = NULL;
  #endif
#else
  error(AZTECOO_NOT_COMPILED);
#endif
}

template<typename Scalar>
AztecOOSolver<Scalar>::~AztecOOSolver()
{
  _F_
#ifdef HAVE_AZTECOO
  //if (m != NULL) delete m;
  //if (rhs != NULL) delete rhs;
#endif
}

template<typename Scalar>
void AztecOOSolver<Scalar>::set_solver(const char *name)
{
  _F_
#ifdef HAVE_AZTECOO
  int az_solver;
  if (strcasecmp(name, "gmres") == 0) az_solver = AZ_gmres;
  else if (strcasecmp(name, "cg") == 0) az_solver = AZ_cg;
  else if (strcasecmp(name, "cgs") == 0) az_solver = AZ_cgs;
  else if (strcasecmp(name, "tfqmr") == 0) az_solver = AZ_tfqmr;
  else if (strcasecmp(name, "bicgstab") == 0) az_solver = AZ_bicgstab;
  else az_solver = AZ_gmres;

  aztec.SetAztecOption(AZ_solver, az_solver);
#endif
}

template<typename Scalar>
void AztecOOSolver<Scalar>::set_precond(const char *name)
{
  _F_
#ifdef HAVE_AZTECOO
  int az_precond;
  if (strcasecmp(name, "none") == 0) az_precond = AZ_none;
  else if (strcasecmp(name, "jacobi") == 0) az_precond = AZ_Jacobi;
  else if (strcasecmp(name, "neumann") == 0) az_precond = AZ_Neumann;
  else if (strcasecmp(name, "least-squares") == 0) az_precond = AZ_ls;
  else az_precond = AZ_none;
  
  this->precond_yes = (az_precond != AZ_none);
  aztec.SetAztecOption(AZ_precond, az_precond);
#endif
}

template<typename Scalar>
void AztecOOSolver<Scalar>::set_option(int option, int value)
{
  _F_
#ifdef HAVE_AZTECOO
  aztec.SetAztecOption(option, value);
#endif
}

template<typename Scalar>
void AztecOOSolver<Scalar>::set_param(int param, double value)
{
  _F_
#ifdef HAVE_AZTECOO
  aztec.SetAztecParam(param, value);
#endif
}

template<typename Scalar>
bool AztecOOSolver<Scalar>::solve()
{
  _F_
#ifdef HAVE_AZTECOO
  assert(m != NULL);
  assert(rhs != NULL);
  assert(m->size == rhs->size);

  TimePeriod tmr;

  // no output
  aztec.SetAztecOption(AZ_output, AZ_none);	// AZ_all | AZ_warnings | AZ_last | AZ_summary

#ifndef HERMES_COMMON_COMPLEX
  // setup the problem
  aztec.SetUserMatrix(m->mat);
  aztec.SetRHS(rhs->vec);
  Epetra_Vector x(*rhs->std_map);
  aztec.SetLHS(&x);

#ifdef HAVE_TEUCHOS
  if (!pc.is_null())
#else
  if (pc != NULL)
#endif
  {
    Epetra_Operator *op = pc->get_obj();
    assert(op != NULL);		// can work only with Epetra_Operators
    aztec.SetPrecOperator(op);
  }

  // solve it
  aztec.Iterate(this->max_iters, this->tolerance);

  tmr.tick();
  this->time = tmr.accumulated();

  delete [] this->sln;
  this->sln = new Scalar[m->size];
  MEM_CHECK(this->sln);
  memset(this->sln, 0, m->size * sizeof(Scalar));

  // copy the solution into sln vector
  for (unsigned int i = 0; i < m->size; i++) this->sln[i] = x[i];
#else
  double c0r = 1.0, c0i = 0.0;
  double c1r = 0.0, c1i = 1.0;

  Epetra_Vector xr(*rhs->std_map);
  Epetra_Vector xi(*rhs->std_map);

  Komplex_LinearProblem kp(c0r, c0i, *m->mat, c1r, c1i, *m->mat_im, xr, xi, *rhs->vec, *rhs->vec_im);
  Epetra_LinearProblem *lp = kp.KomplexProblem();
  aztec.SetProblem(*lp);

  // solve it
  aztec.Iterate(this->max_iters, this->tolerance);

  kp.ExtractSolution(xr, xi);

  delete [] this->sln;
  this->sln = new Scalar[m->size];
  MEM_CHECK(this->sln);
  memset(this->sln, 0, m->size * sizeof(Scalar));

  // copy the solution into sln vector
  for (unsigned int i = 0; i < m->size; i++) this->sln[i] = Scalar(xr[i], xi[i]);
#endif
  return true;
#else
  return false;
#endif
}

template<typename Scalar>
int AztecOOSolver<Scalar>::get_num_iters()
{
  _F_
#ifdef HAVE_AZTECOO
  return aztec.NumIters();
#else
  return -1;
#endif
}

template<typename Scalar>
double AztecOOSolver<Scalar>::get_residual()
{
  _F_
#ifdef HAVE_AZTECOO
  return aztec.TrueResidual();
#else
  return -1.0;
#endif
}

template class HERMES_API AztecOOSolver<double>;
template class HERMES_API AztecOOSolver<std::complex<double> >;
