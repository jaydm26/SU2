/*!
 * \file CPrimalGrid.cpp
 * \brief Main classes for defining the primal grid elements
 * \author F. Palacios
 * \version 7.0.3 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2020, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../include/geometry/primal_grid/CPrimalGrid.hpp"
#include "../../../include/geometry/elements/CElement.hpp"

unsigned short CPrimalGrid::nDim;

CPrimalGrid::CPrimalGrid(void) {

  /*--- Set the default values for the pointers ---*/
  Nodes = NULL;
  Neighbor_Elements = NULL;
  ElementOwnsFace = NULL;
  PeriodIndexNeighbors = NULL;
  Coord_CG = NULL;
  Coord_FaceElems_CG = NULL;
  JacobianFaceIsConstant = NULL;
  GlobalIndex = 0;
  
  nProcElemIsOnlyInterpolDonor = 0;
  ProcElemIsOnlyInterpolDonor  = NULL;

}

CPrimalGrid::~CPrimalGrid() {

 if (Nodes != NULL) delete[] Nodes;
 if (Coord_CG != NULL) delete[] Coord_CG;
 if (Neighbor_Elements != NULL) delete[] Neighbor_Elements;
 if (ElementOwnsFace != NULL) delete[] ElementOwnsFace;
 if (PeriodIndexNeighbors != NULL) delete[] PeriodIndexNeighbors;
 if (JacobianFaceIsConstant != NULL) delete[] JacobianFaceIsConstant;
  if (ProcElemIsOnlyInterpolDonor != NULL) delete[] ProcElemIsOnlyInterpolDonor;
}

void CPrimalGrid::SetCoord_CG(su2double **val_coord) {
  unsigned short iDim, iNode, NodeFace, iFace;

  AD::StartPreacc();
  AD::SetPreaccIn(val_coord, GetnNodes(), nDim);

  // for (iDim = 0; iDim < nDim; iDim++) {
  //   Coord_CG[iDim] = 0.0;
  //   for (iNode = 0; iNode < GetnNodes();  iNode++)
  //     Coord_CG[iDim] += val_coord[iNode][iDim]/su2double(GetnNodes());
  // }

  for (iFace = 0; iFace < GetnFaces();  iFace++)
    for (iDim = 0; iDim < nDim; iDim++) {
      Coord_FaceElems_CG[iFace][iDim] = 0.0;
      for (iNode = 0; iNode < GetnNodesFace(iFace); iNode++) {
        NodeFace = GetFaces(iFace, iNode);
        Coord_FaceElems_CG[iFace][iDim] += val_coord[NodeFace][iDim]/su2double(GetnNodesFace(iFace));
      }
    }

  CElement *elements[2] = {nullptr, nullptr};
  if (nDim==3) {
    elements[0] = new CTRIA1();
    elements[1] = new CQUAD4();
  }
  su2double TotalArea = 0, MaxArea = 0, *Area = new su2double[GetnFaces()];
  for (iFace = 0; iFace < GetnFaces(); iFace++) {
    if (nDim==3) {
      CElement* element = elements[GetnNodesFace(iFace)-3];
      for (iNode=0; iNode<GetnNodesFace(iFace); ++iNode) {
        NodeFace = GetFaces(iFace, iNode);
        for (iDim=0; iDim<nDim; ++iDim) {
          element->SetRef_Coord(iNode, iDim, val_coord[NodeFace][iDim]);
        }
      }
      Area[iFace] = element->ComputeArea();
    }
    else {
      Area[iFace] = sqrt(pow(val_coord[1][0]-val_coord[0][0],2)
                        +pow(val_coord[1][1]-val_coord[0][1],2));
    }
    MaxArea = max(Area[iFace],MaxArea)
  }

  for (iFace = 0; iFace < GetnFaces(); iFace++) {
    Area[iFace] = pow(Area[iFace]/MaxArea,2.0);
    TotalArea += Area[iFace];
  }



  for (iDim = 0; iDim < nDim; iDim++) {
    Coord_CG[iDim] = 0.0;
    for (iFace = 0; iFace < GetnFaces();  iFace++)
      Coord_CG[iDim] += Coord_FaceElems_CG[iFace][iDim]*Area[iFace]/TotalArea;
  }

  if (nDim==3) {
    delete elements[0];
    delete elements[1];
  }

  delete [] Area;

  AD::SetPreaccOut(Coord_CG, nDim);
  AD::SetPreaccOut(Coord_FaceElems_CG, GetnFaces(), nDim);
  AD::EndPreacc();

}

void CPrimalGrid::GetAllNeighbor_Elements() {
  cout << "( ";
  for (unsigned short iFace = 0; iFace < GetnFaces(); iFace++)
  {
    cout << GetNeighbor_Elements(iFace) << ", ";
  }
  cout << ")"  << endl;
}

void CPrimalGrid::InitializeJacobianConstantFaces(unsigned short val_nFaces) {

  /*--- Allocate the memory for JacobianFaceIsConstant and initialize
        its values to false.     ---*/
  JacobianFaceIsConstant = new bool[val_nFaces];
  for(unsigned short i=0; i<val_nFaces; ++i)
    JacobianFaceIsConstant[i] = false;
}

void CPrimalGrid::AddProcElemIsOnlyInterpolDonor(unsigned long procInterpol) {

  /*--- First check if the processor is not stored already. ---*/
  bool alreadyStored = false;
  for(unsigned short iProc=0; iProc<nProcElemIsOnlyInterpolDonor; ++iProc)
    if(ProcElemIsOnlyInterpolDonor[iProc] == procInterpol) alreadyStored = true;

  if( !alreadyStored ) {

    /*--- Check for any previously stored processors. In that case
          a reallocation must be carried out. ---*/
    if( nProcElemIsOnlyInterpolDonor ) {

      /* Copy the old ones. */
      unsigned long *tmpProc = new unsigned long[nProcElemIsOnlyInterpolDonor];
      for(unsigned short iProc=0; iProc<nProcElemIsOnlyInterpolDonor; ++iProc)
        tmpProc[iProc] = ProcElemIsOnlyInterpolDonor[iProc];

      /* Reallocate the memory for ProcElemIsInterpolDonor. */
      delete[] ProcElemIsOnlyInterpolDonor;
      ProcElemIsOnlyInterpolDonor = new unsigned long[nProcElemIsOnlyInterpolDonor+1];

      /* Copy the data back and release tmpProc again. */
      for(unsigned short iProc=0; iProc<nProcElemIsOnlyInterpolDonor; ++iProc)
         ProcElemIsOnlyInterpolDonor[iProc] = tmpProc[iProc];
      delete[] tmpProc;
    }
    else {

       /* No previously stored processors. Just allocate the memory. */
       ProcElemIsOnlyInterpolDonor = new unsigned long[1];
    }

    /*--- Store the new processor. ---*/
    ProcElemIsOnlyInterpolDonor[nProcElemIsOnlyInterpolDonor] = procInterpol;
    ++nProcElemIsOnlyInterpolDonor;
  }
}

void CPrimalGrid::InitializeNeighbors(unsigned short val_nFaces) {

  /*--- Allocate the memory for Neighbor_Elements and PeriodIndexNeighbors and
        initialize the arrays to -1 to indicate that no neighbor is present and
        that no periodic transformation is needed to the neighbor. ---*/
  Neighbor_Elements    = new long[val_nFaces];
  ElementOwnsFace      = new bool[val_nFaces];
  PeriodIndexNeighbors = new short[val_nFaces];

  for(unsigned short i=0; i<val_nFaces; ++i) {
    Neighbor_Elements[i]    = -1;
    ElementOwnsFace[i]      =  false;
    PeriodIndexNeighbors[i] = -1;
  }
}
