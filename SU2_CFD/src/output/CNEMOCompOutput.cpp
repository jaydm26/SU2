﻿/*!
 * \file CNEMOCompOutput.cpp
 * \brief Main subroutines for compressible flow output
 * \author W. Maier, R. Sanchez
 * \version 7.0.0 "Blackbird"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
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

#include "../../include/output/CNEMOCompOutput.hpp"

#include "../../../Common/include/geometry/CGeometry.hpp"
#include "../../include/solvers/CSolver.hpp"

CNEMOCompOutput::CNEMOCompOutput(CConfig *config, unsigned short nDim) : CFlowOutput(config, nDim, false) {

  turb_model = config->GetKind_Turb_Model();
  lastInnerIter = curInnerIter;
  gridMovement = config->GetGrid_Movement();

  /*--- Set the default history fields if nothing is set in the config file ---*/

  if (nRequestedHistoryFields == 0){
    requestedHistoryFields.emplace_back("ITER");
    requestedHistoryFields.emplace_back("RMS_RES");
    nRequestedHistoryFields = requestedHistoryFields.size();
  }
  if (nRequestedScreenFields == 0){
    if (config->GetTime_Domain()) requestedScreenFields.emplace_back("TIME_ITER");
    if (multiZone) requestedScreenFields.emplace_back("OUTER_ITER");
    requestedScreenFields.emplace_back("INNER_ITER");
    requestedScreenFields.emplace_back("RMS_DENSITY_N2");
    requestedScreenFields.emplace_back("RMS_DENSITY_O2");
    requestedScreenFields.emplace_back("RMS_DENSITY_NO");
    requestedScreenFields.emplace_back("RMS_DENSITY_N");
    requestedScreenFields.emplace_back("RMS_DENSITY_O");
    requestedScreenFields.emplace_back("RMS_MOMENTUM-X");
    requestedScreenFields.emplace_back("RMS_MOMENTUM-Y");
    requestedScreenFields.emplace_back("RMS_ENERGY");
    requestedScreenFields.emplace_back("RMS_ENERGY_VE");
    nRequestedScreenFields = requestedScreenFields.size();
  }
  if (nRequestedVolumeFields == 0){
    requestedVolumeFields.emplace_back("COORDINATES");
    requestedVolumeFields.emplace_back("SOLUTION");
    requestedVolumeFields.emplace_back("PRIMITIVE");
    nRequestedVolumeFields = requestedVolumeFields.size();
  }

  stringstream ss;
  ss << "Zone " << config->GetiZone() << " (Comp. Fluid)";
  multiZoneHeaderString = ss.str();

  /*--- Set the volume filename --- */

  volumeFilename = config->GetVolume_FileName();

  /*--- Set the surface filename --- */

  surfaceFilename = config->GetSurfCoeff_FileName();

  /*--- Set the restart filename --- */

  restartFilename = config->GetRestart_FileName();

  /*--- Set the default convergence field --- */

  if (convFields.empty() ) convFields.emplace_back("RMS_DENSITY");

  if (config->GetFixed_CL_Mode()) {
    bool found = false;
    for (unsigned short iField = 0; iField < convFields.size(); iField++)
      if (convFields[iField] == "LIFT") found = true;
    if (!found) {
      if (rank == MASTER_NODE)
        cout<<"  Fixed CL: Adding LIFT as Convergence Field to ensure convergence to target CL"<<endl;
      convFields.emplace_back("LIFT");
      newFunc.resize(convFields.size());
      oldFunc.resize(convFields.size());
      cauchySerie.resize(convFields.size(), vector<su2double>(nCauchy_Elems, 0.0));
    }
  }
}

CNEMOCompOutput::~CNEMOCompOutput(void) {}

void CNEMOCompOutput::SetHistoryOutputFields(CConfig *config){

  unsigned short nSpecies = config -> GetnSpecies();

  /// BEGIN_GROUP: RMS_RES, DESCRIPTION: The root-mean-square residuals of the SOLUTION variables.
  if (nSpecies == 2){
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_N2",   "rms[Rho_N2]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the N2 density.", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_N",    "rms[Rho_N]",   ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the N density.", HistoryFieldType::RESIDUAL);
  }
  if (nSpecies == 5){
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_N2",  "rms[Rho_N2]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the N2 density.", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_O2",  "rms[Rho_O2]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the O2 density.", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_NO",  "rms[Rho_NO]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the NO density.", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_N",   "rms[Rho_N]",   ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the N density.", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the density.
    AddHistoryOutput("RMS_DENSITY_O",   "rms[Rho_O]",   ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the O density.", HistoryFieldType::RESIDUAL);
  }
  /// DESCRIPTION: Root-mean square residual of the momentum x-component.
  AddHistoryOutput("RMS_MOMENTUM-X", "rms[RhoU]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the momentum x-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the momentum y-component.
  AddHistoryOutput("RMS_MOMENTUM-Y", "rms[RhoV]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the momentum y-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the momentum z-component.
  if (nDim == 3) AddHistoryOutput("RMS_MOMENTUM-Z", "rms[RhoW]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the momentum z-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the energy.
  AddHistoryOutput("RMS_ENERGY",     "rms[RhoE]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the energy.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the energy.
  AddHistoryOutput("RMS_ENERGY_VE",  "rms[RhoEve]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of the energy.", HistoryFieldType::RESIDUAL);

  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Root-mean square residual of nu tilde (SA model).
    AddHistoryOutput("RMS_NU_TILDE", "rms[nu]", ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of nu tilde (SA model).", HistoryFieldType::RESIDUAL);
    break;
  case SST: case SST_SUST:
    /// DESCRIPTION: Root-mean square residual of kinetic energy (SST model).
    AddHistoryOutput("RMS_TKE", "rms[k]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of kinetic energy (SST model).", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the dissipation (SST model).
    AddHistoryOutput("RMS_DISSIPATION", "rms[w]",  ScreenOutputFormat::FIXED, "RMS_RES", "Root-mean square residual of dissipation (SST model).", HistoryFieldType::RESIDUAL);
    break;
  default: break;
  }
  /// END_GROUP

  /// BEGIN_GROUP: MAX_RES, DESCRIPTION: The maximum residuals of the SOLUTION variables.
  /// DESCRIPTION: Maximum residual of the density.
  AddHistoryOutput("MAX_DENSITY",    "max[Rho]",  ScreenOutputFormat::FIXED,   "MAX_RES", "Maximum square residual of the density.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum x-component.
  AddHistoryOutput("MAX_MOMENTUM-X", "max[RhoU]", ScreenOutputFormat::FIXED,   "MAX_RES", "Maximum square residual of the momentum x-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum y-component.
  AddHistoryOutput("MAX_MOMENTUM-Y", "max[RhoV]", ScreenOutputFormat::FIXED,   "MAX_RES", "Maximum square residual of the momentum y-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum z-component.
  if (nDim == 3) AddHistoryOutput("MAX_MOMENTUM-Z", "max[RhoW]", ScreenOutputFormat::FIXED,"MAX_RES", "Maximum residual of the z-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the energy.
  AddHistoryOutput("MAX_ENERGY",     "max[RhoE]", ScreenOutputFormat::FIXED,   "MAX_RES", "Maximum residual of the energy.", HistoryFieldType::RESIDUAL);

  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Maximum residual of nu tilde (SA model).
    AddHistoryOutput("MAX_NU_TILDE",       "max[nu]", ScreenOutputFormat::FIXED, "MAX_RES", "Maximum residual of nu tilde (SA model).", HistoryFieldType::RESIDUAL);
    break;
  case SST: case SST_SUST:
    /// DESCRIPTION: Maximum residual of kinetic energy (SST model).
    AddHistoryOutput("MAX_TKE", "max[k]",  ScreenOutputFormat::FIXED, "MAX_RES", "Maximum residual of kinetic energy (SST model).", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Maximum residual of the dissipation (SST model).
    AddHistoryOutput("MAX_DISSIPATION",    "max[w]",  ScreenOutputFormat::FIXED, "MAX_RES", "Maximum residual of dissipation (SST model).", HistoryFieldType::RESIDUAL);
    break;
  default: break;
  }
  /// END_GROUP

  /// BEGIN_GROUP: BGS_RES, DESCRIPTION: The block Gauss Seidel residuals of the SOLUTION variables.
  /// DESCRIPTION: Maximum residual of the density.
  AddHistoryOutput("BGS_DENSITY",    "bgs[Rho]",  ScreenOutputFormat::FIXED,   "BGS_RES", "BGS residual of the density.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum x-component.
  AddHistoryOutput("BGS_MOMENTUM-X", "bgs[RhoU]", ScreenOutputFormat::FIXED,   "BGS_RES", "BGS residual of the momentum x-component.", HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum y-component.
  AddHistoryOutput("BGS_MOMENTUM-Y", "bgs[RhoV]", ScreenOutputFormat::FIXED,   "BGS_RES", "BGS residual of the momentum y-component.",  HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum z-component.
  if (nDim == 3) AddHistoryOutput("BGS_MOMENTUM-Z", "bgs[RhoW]", ScreenOutputFormat::FIXED, "BGS_RES", "BGS residual of the z-component.",  HistoryFieldType::RESIDUAL);
  /// DESCRIPTION: Maximum residual of the energy.
  AddHistoryOutput("BGS_ENERGY",     "bgs[RhoE]", ScreenOutputFormat::FIXED,   "BGS_RES", "BGS residual of the energy.",  HistoryFieldType::RESIDUAL);

  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Maximum residual of nu tilde (SA model).
    AddHistoryOutput("BGS_NU_TILDE",       "bgs[nu]", ScreenOutputFormat::FIXED, "BGS_RES", "BGS residual of nu tilde (SA model).",  HistoryFieldType::RESIDUAL);
    break;
  case SST: case SST_SUST:
    /// DESCRIPTION: Maximum residual of kinetic energy (SST model).
    AddHistoryOutput("BGS_TKE", "bgs[k]",  ScreenOutputFormat::FIXED, "BGS_RES", "BGS residual of kinetic energy (SST model).",  HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: Maximum residual of the dissipation (SST model).
    AddHistoryOutput("BGS_DISSIPATION",    "bgs[w]",  ScreenOutputFormat::FIXED, "BGS_RES", "BGS residual of dissipation (SST model).", HistoryFieldType::RESIDUAL);
    break;
  default: break;
  }
  /// END_GROUP

  vector<string> Marker_Monitoring;
  for (unsigned short iMarker_Monitoring = 0; iMarker_Monitoring < config->GetnMarker_Monitoring(); iMarker_Monitoring++){
    Marker_Monitoring.push_back(config->GetMarker_Monitoring_TagBound(iMarker_Monitoring));
  }
  /// BEGIN_GROUP: AEROELASTIC, DESCRIPTION: Aeroelastic plunge, pitch
  /// DESCRIPTION: Aeroelastic plunge
  AddHistoryOutputPerSurface("PLUNGE", "plunge", ScreenOutputFormat::FIXED, "AEROELASTIC", Marker_Monitoring, HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Aeroelastic pitch
  AddHistoryOutputPerSurface("PITCH",  "pitch",  ScreenOutputFormat::FIXED, "AEROELASTIC", Marker_Monitoring, HistoryFieldType::COEFFICIENT);
  /// END_GROUP


  /// DESCRIPTION: Linear solver iterations
  AddHistoryOutput("LINSOL_ITER", "Linear_Solver_Iterations", ScreenOutputFormat::INTEGER, "LINSOL", "Number of iterations of the linear solver.");
  AddHistoryOutput("LINSOL_RESIDUAL", "LinSolRes", ScreenOutputFormat::FIXED, "LINSOL", "Residual of the linear solver.");

  /// BEGIN_GROUP: ENGINE_OUTPUT, DESCRIPTION: Engine output
  /// DESCRIPTION: Aero CD drag
  AddHistoryOutput("AEROCDRAG",                  "AeroCDrag",                  ScreenOutputFormat::SCIENTIFIC, "ENGINE_OUTPUT", "Aero CD drag", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Solid CD drag
  AddHistoryOutput("SOLIDCDRAG",                 "SolidCDrag",                 ScreenOutputFormat::SCIENTIFIC, "ENGINE_OUTPUT", "Solid CD drag ", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Radial distortion
  AddHistoryOutput("RADIAL_DISTORTION",          "Radial_Distortion",          ScreenOutputFormat::SCIENTIFIC, "ENGINE_OUTPUT", "Radial distortion ", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Circumferential distortion
  AddHistoryOutput("CIRCUMFERENTIAL_DISTORTION", "Circumferential_Distortion", ScreenOutputFormat::SCIENTIFIC, "ENGINE_OUTPUT", "Circumferential distortion", HistoryFieldType::COEFFICIENT);
  /// END_GROUP

  /// BEGIN_GROUP: ROTATING_FRAME, DESCRIPTION: Coefficients related to a rotating frame of reference.
  /// DESCRIPTION: Merit
  AddHistoryOutput("MERIT", "CMerit", ScreenOutputFormat::SCIENTIFIC, "ROTATING_FRAME", "Merit", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: CT
  AddHistoryOutput("CT",    "CT",     ScreenOutputFormat::SCIENTIFIC, "ROTATING_FRAME", "CT", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: CQ
  AddHistoryOutput("CQ",    "CQ",     ScreenOutputFormat::SCIENTIFIC, "ROTATING_FRAME", "CQ", HistoryFieldType::COEFFICIENT);
  /// END_GROUP

  /// BEGIN_GROUP: EQUIVALENT_AREA, DESCRIPTION: Equivalent area.
  /// DESCRIPTION: Equivalent area
  AddHistoryOutput("EQUIV_AREA",   "CEquiv_Area",  ScreenOutputFormat::SCIENTIFIC, "EQUIVALENT_AREA", "Equivalent area", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Nearfield obj. function
  AddHistoryOutput("NEARFIELD_OF", "CNearFieldOF", ScreenOutputFormat::SCIENTIFIC, "EQUIVALENT_AREA", "Nearfield obj. function ", HistoryFieldType::COEFFICIENT);
  /// END_GROUP

  ///   /// BEGIN_GROUP: HEAT_COEFF, DESCRIPTION: Heat coefficients on all surfaces set with MARKER_MONITORING.
  /// DESCRIPTION: Total heatflux
  AddHistoryOutput("HEATFLUX", "HF",      ScreenOutputFormat::SCIENTIFIC, "HEAT", "Total heatflux on all surfaces set with MARKER_MONITORING.", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Maximal heatflux
  AddHistoryOutput("HEATFLUX_MAX", "maxHF",    ScreenOutputFormat::SCIENTIFIC, "HEAT", "Total maximum heatflux on all surfaces set with MARKER_MONITORING.", HistoryFieldType::COEFFICIENT);
  /// DESCRIPTION: Temperature
  AddHistoryOutput("TEMPERATURE", "Temp", ScreenOutputFormat::SCIENTIFIC, "HEAT",  "Total avg. temperature on all surfaces set with MARKER_MONITORING.", HistoryFieldType::COEFFICIENT);
  /// END_GROUP

  AddHistoryOutput("CFL_NUMBER", "CFL number", ScreenOutputFormat::SCIENTIFIC, "CFL_NUMBER", "Current value of the CFL number");

  ///   /// BEGIN_GROUP: FIXED_CL, DESCRIPTION: Relevant outputs for the Fixed CL mode
  if (config->GetFixed_CL_Mode()){
    /// DESCRIPTION: Difference between current and target CL
    AddHistoryOutput("DELTA_CL", "Delta_CL", ScreenOutputFormat::SCIENTIFIC, "FIXED_CL", "Difference between Target CL and current CL", HistoryFieldType::COEFFICIENT);
    /// DESCRIPTION: Angle of attack before the most recent update
    AddHistoryOutput("PREV_AOA", "Previous_AOA", ScreenOutputFormat::FIXED, "FIXED_CL", "Angle of Attack at the previous iteration of the Fixed CL driver");
    /// DESCRIPTION: Last change in angle of attack by the Fixed CL driver
    AddHistoryOutput("CHANGE_IN_AOA", "Change_in_AOA", ScreenOutputFormat::SCIENTIFIC, "FIXED_CL", "Last change in Angle of Attack by Fixed CL Driver", HistoryFieldType::RESIDUAL);
    /// DESCRIPTION: AOA control command by the CL Driver
    AddHistoryOutput("CL_DRIVER_COMMAND", "CL_Driver_Command", ScreenOutputFormat::SCIENTIFIC, "FIXED_CL", "CL Driver's control command", HistoryFieldType::RESIDUAL);
  }

  if (config->GetDeform_Mesh()){
    AddHistoryOutput("DEFORM_MIN_VOLUME", "MinVolume", ScreenOutputFormat::SCIENTIFIC, "DEFORM", "Minimum volume in the mesh");
    AddHistoryOutput("DEFORM_MAX_VOLUME", "MaxVolume", ScreenOutputFormat::SCIENTIFIC, "DEFORM", "Maximum volume in the mesh");
    AddHistoryOutput("DEFORM_ITER", "DeformIter", ScreenOutputFormat::INTEGER, "DEFORM", "Linear solver iterations for the mesh deformation");
    AddHistoryOutput("DEFORM_RESIDUAL", "DeformRes", ScreenOutputFormat::FIXED, "DEFORM", "Residual of the linear solver for the mesh deformation");
  }

  /*--- Add analyze surface history fields --- */

  AddAnalyzeSurfaceOutput(config);

  /*--- Add aerodynamic coefficients fields --- */

  AddAerodynamicCoefficients(config);

  /*--- Add Cp diff fields ---*/

  Add_CpInverseDesignOutput(config);

  /*--- Add combo obj value --- */

  AddHistoryOutput("COMBO", "ComboObj", ScreenOutputFormat::SCIENTIFIC, "COMBO", "Combined obj. function value.", HistoryFieldType::COEFFICIENT);
}

void CNEMOCompOutput::SetVolumeOutputFields(CConfig *config){

  unsigned short nSpecies = config->GetnSpecies();

  // Grid coordinates
  AddVolumeOutput("COORD-X", "x", "COORDINATES", "x-component of the coordinate vector");
  AddVolumeOutput("COORD-Y", "y", "COORDINATES", "y-component of the coordinate vector");
  if (nDim == 3)
    AddVolumeOutput("COORD-Z", "z", "COORDINATES", "z-component of the coordinate vector");

  // Solution variables
  if (nSpecies == 2){
    AddVolumeOutput("DENSITY_N2",  "Density_N2",  "SOLUTION", "Density_N2");
    AddVolumeOutput("DENSITY_N",   "Density_N",   "SOLUTION", "Density_N");
  }
  if (nSpecies == 5){
    AddVolumeOutput("DENSITY_N2",  "Density_N2",  "SOLUTION", "Density_N2");
    AddVolumeOutput("DENSITY_O2",  "Density_O2",  "SOLUTION", "Density_N");
    AddVolumeOutput("DENSITY_NO",  "Density_NO",  "SOLUTION", "Density_NO");
    AddVolumeOutput("DENSITY_N",   "Density_N",   "SOLUTION", "Density_N");
    AddVolumeOutput("DENSITY_O",   "Density_O",   "SOLUTION", "Density_O");
  }

  AddVolumeOutput("MOMENTUM-X", "Momentum_x", "SOLUTION", "x-component of the momentum vector");
  AddVolumeOutput("MOMENTUM-Y", "Momentum_y", "SOLUTION", "y-component of the momentum vector");
  if (nDim == 3)
    AddVolumeOutput("MOMENTUM-Z", "Momentum_z", "SOLUTION", "z-component of the momentum vector");
  AddVolumeOutput("ENERGY",       "Energy",     "SOLUTION", "Energy");
  AddVolumeOutput("ENERGY_VE",    "Energy_ve",  "SOLUTION", "Energy_ve");
  // Turbulent Residuals
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("TKE", "Turb_Kin_Energy", "SOLUTION", "Turbulent kinetic energy");
    AddVolumeOutput("DISSIPATION", "Omega", "SOLUTION", "Rate of dissipation");
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    AddVolumeOutput("NU_TILDE", "Nu_Tilde", "SOLUTION", "Spalart-Allmaras variable");
    break;
  case NONE:
    break;
  }

  // Grid velocity
  if (config->GetGrid_Movement()){
    AddVolumeOutput("GRID_VELOCITY-X", "Grid_Velocity_x", "GRID_VELOCITY", "x-component of the grid velocity vector");
    AddVolumeOutput("GRID_VELOCITY-Y", "Grid_Velocity_y", "GRID_VELOCITY", "y-component of the grid velocity vector");
    if (nDim == 3 )
      AddVolumeOutput("GRID_VELOCITY-Z", "Grid_Velocity_z", "GRID_VELOCITY", "z-component of the grid velocity vector");
  }

  // Primitive variables
  AddVolumeOutput("PRESSURE",       "Pressure",                "PRIMITIVE", "Pressure");
  AddVolumeOutput("TEMPERATURE",    "Temperature",             "PRIMITIVE", "Temperature");
  AddVolumeOutput("TEMPERATURE_VE", "Temperature_ve",          "PRIMITIVE", "Temperature_ve");

  AddVolumeOutput("MACH",        "Mach",                    "PRIMITIVE", "Mach number");
//  AddVolumeOutput("PRESSURE_COEFF", "Pressure_Coefficient", "PRIMITIVE", "Pressure coefficient");

  if (/*config->GetKind_Solver() == NEMO_RANS ||*/ config->GetKind_Solver() == NEMO_NAVIER_STOKES){
    AddVolumeOutput("LAMINAR_VISCOSITY", "Laminar_Viscosity", "PRIMITIVE", "Laminar viscosity");

    //AddVolumeOutput("SKIN_FRICTION-X", "Skin_Friction_Coefficient_x", "PRIMITIVE", "x-component of the skin friction vector");
    //AddVolumeOutput("SKIN_FRICTION-Y", "Skin_Friction_Coefficient_y", "PRIMITIVE", "y-component of the skin friction vector");
    //if (nDim == 3)
    //  AddVolumeOutput("SKIN_FRICTION-Z", "Skin_Friction_Coefficient_z", "PRIMITIVE", "z-component of the skin friction vector");

    //AddVolumeOutput("HEAT_FLUX", "Heat_Flux", "PRIMITIVE", "Heat-flux");
    //AddVolumeOutput("Y_PLUS", "Y_Plus", "PRIMITIVE", "Non-dim. wall distance (Y-Plus)");

  }

// if (config->GetKind_Solver() == NEMO_RANS) {
//   AddVolumeOutput("EDDY_VISCOSITY", "Eddy_Viscosity", "PRIMITIVE", "Turbulent eddy viscosity");
// }

  if (config->GetKind_Trans_Model() == BC){
    AddVolumeOutput("INTERMITTENCY", "gamma_BC", "INTERMITTENCY", "Intermittency");
  }

  //Residuals
  if (nSpecies == 2){
    AddVolumeOutput("RES_DENSITY_N2", "Residual_Density_N2", "RESIDUAL", "Residual of the N2 density");
    AddVolumeOutput("RES_DENSITY_N",  "Residual_Density_N",  "RESIDUAL", "Residual of the N density");
  }
  if (nSpecies == 5){
    AddVolumeOutput("RES_DENSITY_N2", "Residual_Density_N2", "RESIDUAL", "Residual of the N2 density");
    AddVolumeOutput("RES_DENSITY_O2", "Residual_Density_O2", "RESIDUAL", "Residual of the O2 density");
    AddVolumeOutput("RES_DENSITY_NO", "Residual_Density_NO", "RESIDUAL", "Residual of the NO density");
    AddVolumeOutput("RES_DENSITY_N",  "Residual_Density_N",  "RESIDUAL", "Residual of the N density");
    AddVolumeOutput("RES_DENSITY_O",  "Residual_Density_O",  "RESIDUAL", "Residual of the O density");
  }
  AddVolumeOutput("RES_MOMENTUM-X", "Residual_Momentum_x", "RESIDUAL", "Residual of the x-momentum component");
  AddVolumeOutput("RES_MOMENTUM-Y", "Residual_Momentum_y", "RESIDUAL", "Residual of the y-momentum component");
  if (nDim == 3)
    AddVolumeOutput("RES_MOMENTUM-Z", "Residual_Momentum_z", "RESIDUAL", "Residual of the z-momentum component");
  AddVolumeOutput("RES_ENERGY",    "Residual_Energy",    "RESIDUAL", "Residual of the energy");
  AddVolumeOutput("RES_ENERGY_VE", "Residual_Energy_ve", "RESIDUAL", "Residual of the energy_ve");

  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("RES_TKE", "Residual_TKE", "RESIDUAL", "Residual of turbulent kinetic energy");
    AddVolumeOutput("RES_DISSIPATION", "Residual_Omega", "RESIDUAL", "Residual of the rate of dissipation");
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    AddVolumeOutput("RES_NU_TILDE", "Residual_Nu_Tilde", "RESIDUAL", "Residual of the Spalart-Allmaras variable");
    break;
  case NONE:
    break;
  }

  // Limiter values
  AddVolumeOutput("LIMITER_DENSITY", "Limiter_Density", "LIMITER", "Limiter value of the density");
  AddVolumeOutput("LIMITER_MOMENTUM-X", "Limiter_Momentum_x", "LIMITER", "Limiter value of the x-momentum");
  AddVolumeOutput("LIMITER_MOMENTUM-Y", "Limiter_Momentum_y", "LIMITER", "Limiter value of the y-momentum");
  if (nDim == 3)
    AddVolumeOutput("LIMITER_MOMENTUM-Z", "Limiter_Momentum_z", "LIMITER", "Limiter value of the z-momentum");
  AddVolumeOutput("LIMITER_ENERGY", "Limiter_Energy", "LIMITER", "Limiter value of the energy");

  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("LIMITER_TKE", "Limiter_TKE", "LIMITER", "Limiter value of turb. kinetic energy");
    AddVolumeOutput("LIMITER_DISSIPATION", "Limiter_Omega", "LIMITER", "Limiter value of dissipation rate");
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    AddVolumeOutput("LIMITER_NU_TILDE", "Limiter_Nu_Tilde", "LIMITER", "Limiter value of the Spalart-Allmaras variable");
    break;
  case NONE:
    break;
  }

  // Roe Low Dissipation
  if (config->GetKind_RoeLowDiss() != NO_ROELOWDISS){
    AddVolumeOutput("ROE_DISSIPATION", "Roe_Dissipation", "ROE_DISSIPATION", "Value of the Roe dissipation");
  }

  if(/*config->GetKind_Solver() == NEMO_RANS ||*/ config->GetKind_Solver() == NEMO_NAVIER_STOKES){
    if (nDim == 3){
      AddVolumeOutput("VORTICITY_X", "Vorticity_x", "VORTEX_IDENTIFICATION", "x-component of the vorticity vector");
      AddVolumeOutput("VORTICITY_Y", "Vorticity_y", "VORTEX_IDENTIFICATION", "y-component of the vorticity vector");
      AddVolumeOutput("Q_CRITERION", "Q_Criterion", "VORTEX_IDENTIFICATION", "Value of the Q-Criterion");
    }
    AddVolumeOutput("VORTICITY_Z", "Vorticity_z", "VORTEX_IDENTIFICATION", "z-component of the vorticity vector");
  }

  if (config->GetTime_Domain()){
    SetTimeAveragedFields();
  }
}

void CNEMOCompOutput::LoadVolumeData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint){

  CVariable* Node_Flow = solver[NEMO_SOL]->GetNodes();
  CVariable* Node_Turb = NULL;
  unsigned short nSpecies = config->GetnSpecies();

  if (config->GetKind_Turb_Model() != NONE){
    Node_Turb = solver[TURB_SOL]->GetNodes();
  }

  auto*    Node_Geo  = geometry->nodes;

  SetVolumeOutputValue("COORD-X", iPoint,  Node_Geo->GetCoord(iPoint, 0));
  SetVolumeOutputValue("COORD-Y", iPoint,  Node_Geo->GetCoord(iPoint, 1));
  if (nDim == 3)
    SetVolumeOutputValue("COORD-Z", iPoint, Node_Geo->GetCoord(iPoint, 2));

  if (nSpecies == 2){
    SetVolumeOutputValue("DENSITY_N2",   iPoint, Node_Flow->GetSolution(iPoint, 0));
    SetVolumeOutputValue("DENSITY_N",    iPoint, Node_Flow->GetSolution(iPoint, 1));
  }
  if (nSpecies == 5){
    SetVolumeOutputValue("DENSITY_N2",   iPoint, Node_Flow->GetSolution(iPoint, 0));
    SetVolumeOutputValue("DENSITY_O2",   iPoint, Node_Flow->GetSolution(iPoint, 1));
    SetVolumeOutputValue("DENSITY_NO",   iPoint, Node_Flow->GetSolution(iPoint, 2));
    SetVolumeOutputValue("DENSITY_N",    iPoint, Node_Flow->GetSolution(iPoint, 3));
    SetVolumeOutputValue("DENSITY_O",    iPoint, Node_Flow->GetSolution(iPoint, 4));
  }
  SetVolumeOutputValue("MOMENTUM-X", iPoint, Node_Flow->GetSolution(iPoint, nSpecies));
  SetVolumeOutputValue("MOMENTUM-Y", iPoint, Node_Flow->GetSolution(iPoint, nSpecies+1));
  if (nDim == 3){
    SetVolumeOutputValue("MOMENTUM-Z", iPoint, Node_Flow->GetSolution(iPoint, nSpecies+2));
    SetVolumeOutputValue("ENERGY",     iPoint, Node_Flow->GetSolution(iPoint, nSpecies+3));
    SetVolumeOutputValue("ENERGY_VE",  iPoint, Node_Flow->GetSolution(iPoint, nSpecies+4));
  } else {
    SetVolumeOutputValue("ENERGY",     iPoint, Node_Flow->GetSolution(iPoint, nSpecies+2));
    SetVolumeOutputValue("ENERGY_VE",  iPoint, Node_Flow->GetSolution(iPoint, nSpecies+3));
  }

  // Turbulent Residuals
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("TKE",         iPoint, Node_Turb->GetSolution(iPoint, 0));
    SetVolumeOutputValue("DISSIPATION", iPoint, Node_Turb->GetSolution(iPoint, 1));
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    SetVolumeOutputValue("NU_TILDE", iPoint, Node_Turb->GetSolution(iPoint, 0));
    break;
  case NONE:
    break;
  }

  if (config->GetGrid_Movement()){
    SetVolumeOutputValue("GRID_VELOCITY-X", iPoint, Node_Geo->GetGridVel(iPoint)[0]);
    SetVolumeOutputValue("GRID_VELOCITY-Y", iPoint, Node_Geo->GetGridVel(iPoint)[1]);
    if (nDim == 3)
      SetVolumeOutputValue("GRID_VELOCITY-Z", iPoint, Node_Geo->GetGridVel(iPoint)[2]);
  }

  SetVolumeOutputValue("PRESSURE", iPoint, Node_Flow->GetPressure(iPoint));
  SetVolumeOutputValue("TEMPERATURE", iPoint, Node_Flow->GetTemperature(iPoint));
  SetVolumeOutputValue("TEMPERATURE_VE", iPoint, Node_Flow->GetTemperature_ve(iPoint));
  SetVolumeOutputValue("MACH", iPoint, sqrt(Node_Flow->GetVelocity2(iPoint))/Node_Flow->GetSoundSpeed(iPoint));

  su2double VelMag = 0.0;
  for (unsigned short iDim = 0; iDim < nDim; iDim++){
    VelMag += pow(solver[NEMO_SOL]->GetVelocity_Inf(iDim),2.0);
  }
  su2double factor = 1.0/(0.5*solver[NEMO_SOL]->GetDensity_Inf()*VelMag);
  SetVolumeOutputValue("PRESSURE_COEFF", iPoint, (Node_Flow->GetPressure(iPoint) - solver[NEMO_SOL]->GetPressure_Inf())*factor);

  if (/*config->GetKind_Solver() == NEMO_RANS ||*/ config->GetKind_Solver() == NEMO_NAVIER_STOKES){
    SetVolumeOutputValue("LAMINAR_VISCOSITY", iPoint, Node_Flow->GetLaminarViscosity(iPoint));
  }

 //if (config->GetKind_Solver() == NEMO_RANS) {
 //  SetVolumeOutputValue("EDDY_VISCOSITY", iPoint, Node_Flow->GetEddyViscosity(iPoint));
 //}

  if (config->GetKind_Trans_Model() == BC){
    SetVolumeOutputValue("INTERMITTENCY", iPoint, Node_Turb->GetGammaBC(iPoint));
  }

  if (nSpecies == 2) {
    SetVolumeOutputValue("RES_DENSITY_N2", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 0));
    SetVolumeOutputValue("RES_DENSITY_N",  iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 1));
  }
  if (nSpecies == 5) {
    SetVolumeOutputValue("RES_DENSITY_N2", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 0));
    SetVolumeOutputValue("RES_DENSITY_O2", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 1));
    SetVolumeOutputValue("RES_DENSITY_NO", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 2));
    SetVolumeOutputValue("RES_DENSITY_N",  iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 3));
    SetVolumeOutputValue("RES_DENSITY_O",  iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, 4));
  }

  SetVolumeOutputValue("RES_MOMENTUM-X", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies));
  SetVolumeOutputValue("RES_MOMENTUM-Y", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+1));
  if (nDim == 3){
    SetVolumeOutputValue("RES_MOMENTUM-Z", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+2));
    SetVolumeOutputValue("RES_ENERGY",     iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+3));
    SetVolumeOutputValue("RES_ENERGY_VE",  iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+4));
  } else {
    SetVolumeOutputValue("RES_ENERGY", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+2));
    SetVolumeOutputValue("RES_ENERGY_VE", iPoint, solver[NEMO_SOL]->LinSysRes(iPoint, nSpecies+3));
  }

  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("RES_TKE", iPoint, solver[TURB_SOL]->LinSysRes(iPoint, 0));
    SetVolumeOutputValue("RES_DISSIPATION", iPoint, solver[TURB_SOL]->LinSysRes(iPoint, 1));
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    SetVolumeOutputValue("RES_NU_TILDE", iPoint, solver[TURB_SOL]->LinSysRes(iPoint, 0));
    break;
  case NONE:
    break;
  }

  SetVolumeOutputValue("LIMITER_DENSITY",    iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 0));
  SetVolumeOutputValue("LIMITER_MOMENTUM-X", iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 1));
  SetVolumeOutputValue("LIMITER_MOMENTUM-Y", iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 2));
  if (nDim == 3){
    SetVolumeOutputValue("LIMITER_MOMENTUM-Z", iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 3));
    SetVolumeOutputValue("LIMITER_ENERGY",     iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 4));
  } else {
    SetVolumeOutputValue("LIMITER_ENERGY", iPoint, Node_Flow->GetLimiter_Primitive(iPoint, 3));
  }

  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("LIMITER_TKE",         iPoint, Node_Turb->GetLimiter_Primitive(iPoint, 0));
    SetVolumeOutputValue("LIMITER_DISSIPATION", iPoint, Node_Turb->GetLimiter_Primitive(iPoint, 1));
    break;
  case SA: case SA_COMP: case SA_E:
  case SA_E_COMP: case SA_NEG:
    SetVolumeOutputValue("LIMITER_NU_TILDE", iPoint, Node_Turb->GetLimiter_Primitive(iPoint, 0));
    break;
  case NONE:
    break;
  }

  if (config->GetKind_RoeLowDiss() != NO_ROELOWDISS){
    SetVolumeOutputValue("ROE_DISSIPATION", iPoint, Node_Flow->GetRoe_Dissipation(iPoint));
  }

  if(/*config->GetKind_Solver() == NEMO_RANS ||*/ config->GetKind_Solver() == NEMO_NAVIER_STOKES){
    if (nDim == 3){
      SetVolumeOutputValue("VORTICITY_X", iPoint, Node_Flow->GetVorticity(iPoint)[0]);
      SetVolumeOutputValue("VORTICITY_Y", iPoint, Node_Flow->GetVorticity(iPoint)[1]);
      SetVolumeOutputValue("Q_CRITERION", iPoint, GetQ_Criterion(&(Node_Flow->GetGradient_Primitive(iPoint)[1])));
    }
    SetVolumeOutputValue("VORTICITY_Z", iPoint, Node_Flow->GetVorticity(iPoint)[2]);
  }

  if (config->GetTime_Domain()){
    LoadTimeAveragedData(iPoint, Node_Flow);
  }
}

void CNEMOCompOutput::LoadSurfaceData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint, unsigned short iMarker, unsigned long iVertex){

  if ((config->GetKind_Solver() == NEMO_NAVIER_STOKES) /*|| (config->GetKind_Solver()  == NEMO_RANS)*/) {
    SetVolumeOutputValue("SKIN_FRICTION-X", iPoint, solver[NEMO_SOL]->GetCSkinFriction(iMarker, iVertex, 0));
    SetVolumeOutputValue("SKIN_FRICTION-Y", iPoint, solver[NEMO_SOL]->GetCSkinFriction(iMarker, iVertex, 1));
    if (nDim == 3)
      SetVolumeOutputValue("SKIN_FRICTION-Z", iPoint, solver[NEMO_SOL]->GetCSkinFriction(iMarker, iVertex, 2));

    SetVolumeOutputValue("HEAT_FLUX", iPoint, solver[NEMO_SOL]->GetHeatFlux(iMarker, iVertex));
    SetVolumeOutputValue("Y_PLUS", iPoint, solver[NEMO_SOL]->GetYPlus(iMarker, iVertex));
  }
}

void CNEMOCompOutput::LoadHistoryData(CConfig *config, CGeometry *geometry, CSolver **solver)  {

  CSolver* NEMO_solver = solver[NEMO_SOL];
  CSolver* turb_solver = solver[TURB_SOL];
  CSolver* mesh_solver = solver[MESH_SOL];
  unsigned short nSpecies = config->GetnSpecies();

  if (nSpecies == 2) {
    SetHistoryOutputValue("RMS_DENSITY_N2", log10(NEMO_solver->GetRes_RMS(0)));
    SetHistoryOutputValue("RMS_DENSITY_N",  log10(NEMO_solver->GetRes_RMS(1)));
  }
  if (nSpecies == 5) {
    SetHistoryOutputValue("RMS_DENSITY_N2", log10(NEMO_solver->GetRes_RMS(0)));
    SetHistoryOutputValue("RMS_DENSITY_O2", log10(NEMO_solver->GetRes_RMS(1)));
    SetHistoryOutputValue("RMS_DENSITY_NO", log10(NEMO_solver->GetRes_RMS(2)));
    SetHistoryOutputValue("RMS_DENSITY_N",  log10(NEMO_solver->GetRes_RMS(3)));
    SetHistoryOutputValue("RMS_DENSITY_O",  log10(NEMO_solver->GetRes_RMS(4)));
  }
  SetHistoryOutputValue("RMS_MOMENTUM-X", log10(NEMO_solver->GetRes_RMS(nSpecies)));
  SetHistoryOutputValue("RMS_MOMENTUM-Y", log10(NEMO_solver->GetRes_RMS(nSpecies+1)));
  if (nDim == 2){
    SetHistoryOutputValue("RMS_ENERGY",    log10(NEMO_solver->GetRes_RMS(nSpecies+2)));
    SetHistoryOutputValue("RMS_ENERGY_VE", log10(NEMO_solver->GetRes_RMS(nSpecies+3)));
  } else {
    SetHistoryOutputValue("RMS_MOMENTUM-Z", log10(NEMO_solver->GetRes_RMS(nSpecies+2)));
    SetHistoryOutputValue("RMS_ENERGY",     log10(NEMO_solver->GetRes_RMS(nSpecies+3)));
    SetHistoryOutputValue("RMS_ENERGY_VE",  log10(NEMO_solver->GetRes_RMS(nSpecies+4)));
  }

  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    SetHistoryOutputValue("RMS_NU_TILDE", log10(turb_solver->GetRes_RMS(0)));
    break;
  case SST: case SST_SUST:
    SetHistoryOutputValue("RMS_TKE", log10(turb_solver->GetRes_RMS(0)));
    SetHistoryOutputValue("RMS_DISSIPATION",    log10(turb_solver->GetRes_RMS(1)));
    break;
  default: break;
  }

  SetHistoryOutputValue("MAX_DENSITY", log10(NEMO_solver->GetRes_Max(0)));
  SetHistoryOutputValue("MAX_MOMENTUM-X", log10(NEMO_solver->GetRes_Max(1)));
  SetHistoryOutputValue("MAX_MOMENTUM-Y", log10(NEMO_solver->GetRes_Max(2)));
  if (nDim == 2)
    SetHistoryOutputValue("MAX_ENERGY", log10(NEMO_solver->GetRes_Max(3)));
  else {
    SetHistoryOutputValue("MAX_MOMENTUM-Z", log10(NEMO_solver->GetRes_Max(3)));
    SetHistoryOutputValue("MAX_ENERGY", log10(NEMO_solver->GetRes_Max(4)));
  }

  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    SetHistoryOutputValue("MAX_NU_TILDE", log10(turb_solver->GetRes_Max(0)));
    break;
  case SST: case SST_SUST:
    SetHistoryOutputValue("MAX_TKE", log10(turb_solver->GetRes_Max(0)));
    SetHistoryOutputValue("MAX_DISSIPATION",    log10(turb_solver->GetRes_Max(1)));
    break;
  default: break;
  }

  if (multiZone){
    SetHistoryOutputValue("BGS_DENSITY", log10(NEMO_solver->GetRes_BGS(0)));
    SetHistoryOutputValue("BGS_MOMENTUM-X", log10(NEMO_solver->GetRes_BGS(1)));
    SetHistoryOutputValue("BGS_MOMENTUM-Y", log10(NEMO_solver->GetRes_BGS(2)));
    if (nDim == 2)
      SetHistoryOutputValue("BGS_ENERGY", log10(NEMO_solver->GetRes_BGS(3)));
    else {
      SetHistoryOutputValue("BGS_MOMENTUM-Z", log10(NEMO_solver->GetRes_BGS(3)));
      SetHistoryOutputValue("BGS_ENERGY", log10(NEMO_solver->GetRes_BGS(4)));
    }


    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      SetHistoryOutputValue("BGS_NU_TILDE", log10(turb_solver->GetRes_BGS(0)));
      break;
    case SST:
      SetHistoryOutputValue("BGS_TKE", log10(turb_solver->GetRes_BGS(0)));
      SetHistoryOutputValue("BGS_DISSIPATION",    log10(turb_solver->GetRes_BGS(1)));
      break;
    default: break;
    }
  }

  SetHistoryOutputValue("HEATFLUX",     NEMO_solver->GetTotal_HeatFlux());
  SetHistoryOutputValue("HEATFLUX_MAX", NEMO_solver->GetTotal_MaxHeatFlux());
  SetHistoryOutputValue("TEMPERATURE",  NEMO_solver->GetTotal_AvgTemperature());

  SetHistoryOutputValue("CFL_NUMBER", config->GetCFL(MESH_0));

  SetHistoryOutputValue("LINSOL_ITER", NEMO_solver->GetIterLinSolver());
  SetHistoryOutputValue("LINSOL_RESIDUAL", log10(NEMO_solver->GetResLinSolver()));

  if (config->GetDeform_Mesh()){
    SetHistoryOutputValue("DEFORM_MIN_VOLUME", mesh_solver->GetMinimum_Volume());
    SetHistoryOutputValue("DEFORM_MAX_VOLUME", mesh_solver->GetMaximum_Volume());
    SetHistoryOutputValue("DEFORM_ITER", mesh_solver->GetIterLinSolver());
    SetHistoryOutputValue("DEFORM_RESIDUAL", log10(mesh_solver->GetResLinSolver()));
  }

  if(config->GetFixed_CL_Mode()){
    SetHistoryOutputValue("DELTA_CL", fabs(NEMO_solver->GetTotal_CL() - config->GetTarget_CL()));
    SetHistoryOutputValue("PREV_AOA", NEMO_solver->GetPrevious_AoA());
    SetHistoryOutputValue("CHANGE_IN_AOA", config->GetAoA()-NEMO_solver->GetPrevious_AoA());
    SetHistoryOutputValue("CL_DRIVER_COMMAND", NEMO_solver->GetAoA_inc());

  }

  /*--- Set the analyse surface history values --- */

  SetAnalyzeSurface(NEMO_solver, geometry, config, false);

  /*--- Set aeroydnamic coefficients --- */

  SetAerodynamicCoefficients(config, NEMO_solver);

  /*--- Set Cp diff fields ---*/

  Set_CpInverseDesign(NEMO_solver, geometry, config);

  /*--- Set combo obj value --- */

  SetHistoryOutputValue("COMBO", NEMO_solver->GetTotal_ComboObj());

}

bool CNEMOCompOutput::SetInit_Residuals(CConfig *config){

  return (config->GetTime_Marching() != STEADY && (curInnerIter == 0))||
        (config->GetTime_Marching() == STEADY && (curInnerIter < 2));

}

bool CNEMOCompOutput::SetUpdate_Averages(CConfig *config){

  return (config->GetTime_Marching() != STEADY && (curInnerIter == config->GetnInner_Iter() - 1 || convergence));

}

void CNEMOCompOutput::SetAdditionalScreenOutput(CConfig *config){

  if (config->GetFixed_CL_Mode()){
    SetFixedCLScreenOutput(config);
  }
}

void CNEMOCompOutput::SetFixedCLScreenOutput(CConfig *config){
  PrintingToolbox::CTablePrinter FixedCLSummary(&cout);

  if (fabs(historyOutput_Map["CL_DRIVER_COMMAND"].value) > 1e-16){
    FixedCLSummary.AddColumn("Fixed CL Mode", 40);
    FixedCLSummary.AddColumn("Value", 30);
    FixedCLSummary.SetAlign(PrintingToolbox::CTablePrinter::LEFT);
    FixedCLSummary.PrintHeader();
    FixedCLSummary << "Current CL" << historyOutput_Map["LIFT"].value;
    FixedCLSummary << "Target CL" << config->GetTarget_CL();
    FixedCLSummary << "Previous AOA" << historyOutput_Map["PREV_AOA"].value;
    if (config->GetFinite_Difference_Mode()){
      FixedCLSummary << "Changed AoA by (Finite Difference step)" << historyOutput_Map["CL_DRIVER_COMMAND"].value;
      lastInnerIter = curInnerIter - 1;
    }
    else
      FixedCLSummary << "Changed AoA by" << historyOutput_Map["CL_DRIVER_COMMAND"].value;
    FixedCLSummary.PrintFooter();
    SetScreen_Header(config);
  }

  else if (config->GetFinite_Difference_Mode() && historyOutput_Map["AOA"].value == historyOutput_Map["PREV_AOA"].value){
    FixedCLSummary.AddColumn("Fixed CL Mode (Finite Difference)", 40);
    FixedCLSummary.AddColumn("Value", 30);
    FixedCLSummary.SetAlign(PrintingToolbox::CTablePrinter::LEFT);
    FixedCLSummary.PrintHeader();
    FixedCLSummary << "Delta CL / Delta AoA" << config->GetdCL_dAlpha();
    FixedCLSummary << "Delta CD / Delta CL" << config->GetdCD_dCL();
    if (nDim == 3){
      FixedCLSummary << "Delta CMx / Delta CL" << config->GetdCMx_dCL();
      FixedCLSummary << "Delta CMy / Delta CL" << config->GetdCMy_dCL();
    }
    FixedCLSummary << "Delta CMz / Delta CL" << config->GetdCMz_dCL();
    FixedCLSummary.PrintFooter();
    curInnerIter = lastInnerIter;
    WriteMetaData(config);
    curInnerIter = config->GetInnerIter();
  }
}

bool CNEMOCompOutput::WriteHistoryFile_Output(CConfig *config) {
  return !config->GetFinite_Difference_Mode() && COutput::WriteHistoryFile_Output(config);
}