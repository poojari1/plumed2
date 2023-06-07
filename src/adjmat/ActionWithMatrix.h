/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2023 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_adjmat_ActionWithMatrix_h
#define __PLUMED_adjmat_ActionWithMatrix_h

#include "core/ActionWithVector.h"

namespace PLMD {
namespace adjmat {

class ActionWithMatrix : public ActionWithVector {
private:
  ActionWithMatrix* matrix_to_do_after;
/// This holds the bookeeping arrays for sparse matrices
  std::vector<unsigned> matrix_bookeeping;
/// Update all the neighbour lists in the chain
  void updateAllNeighbourLists();
/// This is used to clear up the matrix elements
  void clearMatrixElements( MultiValue& myvals ) const ;
/// This is used to find the total amount of space we need for storing matrix elements
  void getTotalMatrixBookeeping( unsigned& stashsize ) const ;
/// This transfers the non-zero elements to the Value
  void transferNonZeroMatrixElementsToValues( unsigned& nval, const std::vector<unsigned>& matbook );
/// This does the calculation of a particular matrix element
  void runTask( const std::string& controller, const unsigned& current, const unsigned colno, MultiValue& myvals ) const ;
/// Get the start of the derivatives of the jth argument
  unsigned getDerivativeStart( const unsigned& jarg ) const ;
protected:
/// This tells us if we need to do the inner loop separately for this action
  bool doInnerLoop;
/// This returns the jelem th element of argument ic
  double getArgumentElement( const unsigned& ic, const unsigned& jelem, const MultiValue& myvals ) const ;
/// This returns an element of a matrix that is passed an argument
  double getElementOfMatrixArgument( const unsigned& imat, const unsigned& irow, const unsigned& jcol, const MultiValue& myvals ) const ;
/// Add derivatives given the derivative wrt to the input vector element as input
  void addDerivativeOnVectorArgument( const unsigned& ival, const unsigned& jarg, const unsigned& jelem, const double& der, MultiValue& myvals ) const ;
/// Add derivatives given the derative wrt to the input matrix element as input
  void addDerivativeOnMatrixArgument( const unsigned& ival, const unsigned& jarg, const unsigned& irow, const unsigned& jcol, const double& der, MultiValue& myvals ) const ;
public:
  static void registerKeywords( Keywords& keys );
  explicit ActionWithMatrix(const ActionOptions&);
///
  void finishChainBuild( ActionWithVector* act );
/// This should return the number of columns to help with sparse storage of matrices
  virtual unsigned getNumberOfColumns() const = 0;
/// This requires some thought
  void setupStreamedComponents( unsigned& nquants, unsigned& nmat, unsigned& maxcol, unsigned& nbookeeping ) override;
//// This does some setup before we run over the row of the matrix
  virtual void setupForTask( const unsigned& task_index, std::vector<unsigned>& indices, MultiValue& myvals ) const = 0; 
/// Run over one row of the matrix
  void performTask( const unsigned& task_index, MultiValue& myvals ) const override ;
/// Gather a row of the matrix
  void gatherStoredValue( const unsigned& valindex, const unsigned& code, const MultiValue& myvals, const unsigned& bufstart, std::vector<double>& buffer ) const override;
/// Gather all the data from the threads
  void gatherThreads( const unsigned& nt, const unsigned& bufsize, const std::vector<double>& omp_buffer, std::vector<double>& buffer, MultiValue& myvals ) override ;
/// Gather all the data from the MPI processes
  void gatherProcesses( std::vector<double>& buffer ) override;
/// This is the virtual that will do the calculation of the task for a particular matrix element
  virtual void performTask( const std::string& controller, const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const = 0;
/// This is the jobs that need to be done when we have run all the jobs in a row of the matrix
  virtual void runEndOfRowJobs( const unsigned& ival, const std::vector<unsigned> & indices, MultiValue& myvals ) const = 0;
/// This is overwritten in Adjacency matrix where we have a neighbour list
  virtual void updateNeighbourList() {}
/// Run the calculation
  virtual void calculate() override;
};

inline 
double ActionWithMatrix::getArgumentElement( const unsigned& ic, const unsigned& jelem, const MultiValue& myvals ) const {
  if( !getPntrToArgument(ic)->valueHasBeenSet() ) return myvals.get( getPntrToArgument(ic)->getPositionInStream() );
  return getPntrToArgument(ic)->get( jelem );
}

inline
double ActionWithMatrix::getElementOfMatrixArgument( const unsigned& imat, const unsigned& irow, const unsigned& jcol, const MultiValue& myvals ) const {
  plumed_dbg_assert( imat<getNumberOfArguments() && getPntrToArgument(imat)->getRank()==2 && !getPntrToArgument(imat)->hasDerivatives() );
  if( !getPntrToArgument(imat)->valueHasBeenSet() ) return myvals.get( getPntrToArgument(imat)->getPositionInStream() );
  return getArgumentElement( imat, irow*getPntrToArgument(imat)->getShape()[1] + jcol, myvals );
}

inline
unsigned ActionWithMatrix::getDerivativeStart( const unsigned& jarg ) const {
  unsigned vstart=0; 
  for(unsigned i=0; i<jarg; ++i) {
      if( getPntrToArgument(i)->valueHasBeenSet() ) vstart += getPntrToArgument(i)->getNumberOfValues();
      else vstart += (getPntrToArgument(i)->getPntrToAction())->getNumberOfDerivatives();
  }
  return vstart;
}

inline
void ActionWithMatrix::addDerivativeOnVectorArgument( const unsigned& ival, const unsigned& jarg, const unsigned& jelem, const double& der, MultiValue& myvals ) const {
  plumed_dbg_assert( imat<getNumberOfArguments() && getPntrToArgument(imat)->getRank()==1 && !getPntrToArgument(imat)->hasDerivatives() );
  unsigned ostrn = getConstPntrToComponent(ival)->getPositionInStream(), vstart=getDerivativeStart(jarg);
  if( getPntrToArgument(jarg)->valueHasBeenSet() ) {
      myvals.addDerivative( ostrn, vstart + jelem, der ); myvals.updateIndex( ostrn, vstart + jelem );
  } else plumed_merror("I don't think this is necessary as the vectors we need will be stored");
}

inline
void ActionWithMatrix::addDerivativeOnMatrixArgument( const unsigned& ival, const unsigned& jarg, const unsigned& irow, const unsigned& jcol, const double& der, MultiValue& myvals ) const {
  plumed_dbg_assert( imat<getNumberOfArguments() && getPntrToArgument(imat)->getRank()==2 && !getPntrToArgument(imat)->hasDerivatives() );
  unsigned ostrn = getConstPntrToComponent(ival)->getPositionInStream(), vstart=getDerivativeStart(jarg); 
  if( getPntrToArgument(jarg)->valueHasBeenSet() ) {
      unsigned dloc = vstart + irow*getPntrToArgument(jarg)->getShape()[1] + jcol; 
      myvals.addDerivative( ostrn, dloc, der ); myvals.updateIndex( ostrn, dloc );
  } else {
      unsigned istrn = getPntrToArgument(jarg)->getPositionInStream();
      for(unsigned k=0; k<myvals.getNumberActive(istrn); ++k) {
          unsigned kind=myvals.getActiveIndex(istrn,k);
          myvals.addDerivative( ostrn, kind, der*myvals.getDerivative( istrn, kind ) );
      }
  }
}

}
}
#endif
