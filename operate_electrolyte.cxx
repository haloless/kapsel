/*!
  \file operate_electrolyte.cxx
  \author Y. Nakayama
  \date 2006/06/27
  \version 1.1
  \brief Routines to compute the charge distributions and forces
 */
#include "operate_electrolyte.h"

double Bjerrum_length;
double Surface_ion_number;
double Counterion_number;
double *Valency;
double *Onsager_coeff;

inline void Add_external_electric_field_x(double *potential
					  ,const CTime &jikan
					  ){
  double external[DIM];
  for(int d=0;d<DIM;d++){
    external[d] = E_ext[d];
    if(AC){
      double time = jikan.time;
      external[d] *= sin( Angular_Frequency * time);
    }
  }

	int im;
#pragma omp parallel for schedule(dynamic, 1) private(im)
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ;k++){
		im=(i*NY*NZ_)+(j*NZ_)+k;
	potential[im] -= (
			       (external[0] * (double)i 
				+ external[1] * (double)j 
				+ external[2] * (double)k
				) 
			       * DX);
      }
    }
  }

}
void Conc_k2charge_field(Particle *p
			 ,double **conc_k
			 ,double *charge_density
			 ,double *phi_p // working memory
			 ,double *dmy_value // working memory
			 ){
  {
    Reset_phi(phi_p);
    Reset_phi(charge_density);
    Make_phi_qq_particle(phi_p, charge_density, p);
	int im;
	double dmy_phi;
	double dmy_conc;

    for(int n=0;n< N_spec;n++){
      A_k2a_out(conc_k[n], dmy_value);
#pragma omp parallel for schedule(dynamic, 1) private(im,dmy_phi,dmy_conc)
      for(int i=0;i<NX;i++){
	  for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
		im=(i*NY*NZ_)+(j*NZ_)+k;
	    dmy_phi = phi_p[im];
	    dmy_conc = dmy_value[im];
	    charge_density[im] += Valency_e[n] * dmy_conc * (1.- dmy_phi);
	  }
	}
      }
    }
  }
}

void Charge_field_k2Coulomb_potential_k_PBC(double *potential){
  const double iDielectric_cst = 1./Dielectric_cst;
  int im;
#pragma omp parallel for schedule(dynamic, 1) private(im)
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ_;k++){
	im=(i*NY*NZ_)+(j*NZ_)+k;
	potential[im] *= (IK2[im] * iDielectric_cst);
      }
    }
  }
}

void Make_Coulomb_force_x_on_fluid(double **force
				    ,Particle *p
				    ,double **conc_k
				    ,double *charge_density // working memory
				    ,double *potential // working memory
				    ,const CTime &jikan
				    ){
  Conc_k2charge_field(p, conc_k, charge_density, force[0], force[1]);
  A2a_k_out(charge_density, potential);
  Charge_field_k2Coulomb_potential_k_PBC(potential);

  A_k2da_k(potential, force);
  U_k2u(force);

  if(Fixed_particle){
    Reset_phi(phi);
    Reset_phi(charge_density);
    Make_phi_qq_fixed_particle(phi, charge_density, p);
    for(int n=0;n<N_spec;n++){
      A_k2a_out(conc_k[n],potential);
      for(int i=0; i<NX; i++){
	  for(int j=0; j<NY; j++){
	  for(int k=0; k<NZ; k++){
	    double dmy_phi = phi[(i*NY*NZ_)+(j*NZ_)+k];
	    double dmy_conc = potential[(i*NY*NZ_)+(j*NZ_)+k];
	    charge_density[(i*NY*NZ_)+(j*NZ_)+k] += Valency_e[n] * dmy_conc * (1.- dmy_phi);
	  }
	}
      }
    }
  }

  double external[DIM];
  for(int d=0;d<DIM;d++){
    external[d] = E_ext[d];
    if(AC){
      double time = jikan.time;
      external[d] *= sin( Angular_Frequency * time);
    }
  }
  int im;
  double electric_field;
#pragma omp parallel
  {
//#pragma omp parallel for schedule(dynamic, 1) private(im,electric_field)
#pragma omp  for schedule(dynamic, 1) private(im,electric_field)
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
	  im=(i*NY*NZ_)+(j*NZ_)+k;
	  electric_field = -force[0][im];
	  if(External_field){
	   electric_field += external[0];
	  }
	  force[0][im] = charge_density[im] * electric_field; 
	  }
      }
    }
//#pragma omp parallel for schedule(dynamic, 1) private(im,electric_field)
#pragma omp  for schedule(dynamic, 1) private(im,electric_field)
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
	  im=(i*NY*NZ_)+(j*NZ_)+k;
	  electric_field = -force[1][im];
	  if(External_field){
	   electric_field += external[1];
	  }
	  force[1][im] = charge_density[im] * electric_field; 
	  }
      }
    }
//#pragma omp parallel for schedule(dynamic, 1) private(im,electric_field)
#pragma omp for schedule(dynamic, 1) private(im,electric_field)
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
	  im=(i*NY*NZ_)+(j*NZ_)+k;
	  electric_field = -force[2][im];
	  if(External_field){
	   electric_field += external[2];
	  }
	  force[2][im] = charge_density[im] * electric_field; 
	  }
      }
    }
  }
}

void Make_phi_qq_particle(double *phi
			   ,double *surface
			   ,Particle *p){

  double abs_total_surface_charge = 0.;
  for(int n=0; n < Particle_Number; n++){
    double dmy_surface_charge = Surface_charge_e[p[n].spec];
    double xp[DIM];
    for(int d=0;d<DIM;d++){
      xp[d] = p[n].x[d];
      {
	assert(p[n].x[d] >= 0);
	assert(p[n].x[d] < L[d]);
      }
    }
    
    int x_int[DIM];
    double residue[DIM];
    int sw_in_cell 
      = Particle_cell(xp, DX, x_int, residue);// {1,0} $B$,JV$C$F$/$k(B
    sw_in_cell = 1;
    int r_mesh[DIM];
    double r[DIM];
    for(int mesh=0; mesh < NP_domain; mesh++){
      Relative_coord(Sekibun_cell[mesh], x_int, residue, sw_in_cell, Ns, DX, r_mesh, r);
      double x[DIM];
      for(int d=0;d<DIM;d++){
	x[d] = r_mesh[d] * DX;
      }
      double dmy = Distance(x, xp);
      double dmy_phi= Phi(dmy);
      double dmy_Dphi= DPhi_compact_sin(dmy);

      phi[(r_mesh[0]*NY*NZ_)+(r_mesh[1]*NZ_)+r_mesh[2]] += dmy_phi;
      surface[(r_mesh[0]*NY*NZ_)+(r_mesh[1]*NZ_)+r_mesh[2]] += dmy_surface_charge * dmy_Dphi;
    }
    abs_total_surface_charge += fabs(dmy_surface_charge);
  }
  {// volume of surface section is normalized to unity
    double dmy = 0.0;
	int im;
#pragma omp parallel for schedule(dynamic, 1) reduction(+:dmy) private(im)
    for(int i=0; i<NX; i++){
      for(int j=0; j<NY; j++){
	  for(int k=0; k<NZ; k++){
	  //int im=(i*NY*NZ_)+(j*NZ_)+k;
	  im=(i*NY*NZ_)+(j*NZ_)+k;
	  dmy += fabs(surface[im]);
	  }
      }
    }
    double rescale = abs_total_surface_charge / (dmy * DX3);
#pragma omp parallel for schedule(dynamic, 1) private(im)
    for(int i=0; i<NX; i++){
      for(int j=0; j<NY; j++){
	  for(int k=0; k<NZ; k++){
	  //int im=(i*NY*NZ_)+(j*NZ_)+k;
	  im=(i*NY*NZ_)+(j*NZ_)+k;
	  surface[im] *= rescale; 
	  }
      }
    }
  }
}

void Make_phi_qq_fixed_particle(double *phi
			   ,double *surface
			   ,Particle *p){

  double abs_total_surface_charge = 0.;
  for(int n=0; n < Particle_Number; n++){
    double dmy_surface_charge = Surface_charge_e[p[n].spec];
    double xp[DIM];
    for(int d=0;d<DIM;d++){
      xp[d] = p[n].x[d];
      {
	assert(p[n].x[d] >= 0);
	assert(p[n].x[d] < L[d]);
      }
    }
    
    int x_int[DIM];
    double residue[DIM];
    int sw_in_cell 
      = Particle_cell(xp, DX, x_int, residue);// {1,0} $B$,JV$C$F$/$k(B
    sw_in_cell = 1;
    int r_mesh[DIM];
    double r[DIM];
    for(int mesh=0; mesh < NP_domain; mesh++){
      Relative_coord(Sekibun_cell[mesh], x_int, residue, sw_in_cell, Ns, DX, r_mesh, r);
      double x[DIM];
      for(int d=0;d<DIM;d++){
	x[d] = r_mesh[d] * DX;
      }
      double dmy = Distance(x, xp);
      double dmy_phi= Phi(dmy);
      double dmy_Dphi= DPhi_compact_sin(dmy);

      phi[(r_mesh[0]*NY*NZ_)+(r_mesh[1]*NZ_)+r_mesh[2]] += dmy_phi;
      surface[(r_mesh[0]*NY*NZ_)+(r_mesh[1]*NZ_)+r_mesh[2]] += dmy_surface_charge * dmy_Dphi;
    }
    abs_total_surface_charge += fabs(dmy_surface_charge);
  }
  {
    double dmy = 0.0;
    for(int i=0; i<NX; i++){
      for(int j=0; j<NY; j++){
	for(int k=0; k<NZ; k++){
	  dmy += fabs(surface[(i*NY*NZ_)+(j*NZ_)+k])*phi[(i*NY*NZ_)+(j*NZ_)+k];
	}
      }
    }
    double rescale = abs_total_surface_charge * 0.5 / (dmy * DX3);
    //  fprintf(stderr,"# integral surface  %g\n", dmy*DX3);
    for(int i=0; i<NX; i++){
      for(int j=0; j<NY; j++){
	for(int k=0; k<NZ; k++){
	  surface[(i*NY*NZ_)+(j*NZ_)+k] *= rescale; 
	}
      }
    }
  }
}
void Calc_free_energy_PB(double **conc_k
			  ,Particle *p
			  ,double *free_energy
			  ,double *phi // working memory
			  ,double *charge_density // working memory
			  ,double *dmy_value // working memory
			  ,const CTime &jikan
			  ){

  double free_energy_ideal = 0.;
  double free_energy_electrostatic = 0.;
  int im;
  double dmy_conc;
  double dmy_phi;

  {
    Reset_phi(phi);
    Make_phi_particle(phi, p);
    
    for(int n=0;n< N_spec;n++){
      A_k2a_out(conc_k[n], dmy_value);
#pragma omp parallel for schedule(dynamic, 1) reduction(+:free_energy_ideal) private(im,dmy_conc,dmy_phi)
      for(int i=0;i<NX;i++){
	  for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
		im=(i*NY*NZ_)+(j*NZ_)+k;
	    dmy_conc = dmy_value[im];
	    dmy_phi = phi[im];
	    free_energy_ideal += (1.- dmy_phi) * dmy_conc * (log(dmy_conc)-1.);
	  }
	  }
      }
    }
    free_energy_ideal *= (kBT * DX3);
  }
  {
    Conc_k2charge_field(p, conc_k, charge_density, phi, dmy_value);
    {
      A2a_k_out(charge_density, dmy_value);
      Charge_field_k2Coulomb_potential_k_PBC(dmy_value);
      A_k2a(dmy_value);

#pragma omp parallel for schedule(dynamic, 1) private(im)
      for(int i=0;i<NX;i++){
	  for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
		im=(i*NY*NZ_)+(j*NZ_)+k;
	    dmy_value[im] *= 0.5;
	  }
	  }
      }

      if(External_field){
	Add_external_electric_field_x(dmy_value, jikan);
      }
    }
#pragma omp parallel for schedule(dynamic, 1) reduction(+:free_energy_electrostatic) private(im)
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
		im=(i*NY*NZ_)+(j*NZ_)+k;
	  free_energy_electrostatic += charge_density[im] * dmy_value[im];
	}
      }
    }
    free_energy_electrostatic *= DX3;
  }

  free_energy[1] = free_energy_ideal;
  free_energy[2] = free_energy_electrostatic;
  free_energy[0] = free_energy[1] + free_energy[2];

}

inline void Set_steadystate_ion_density(double **Concentration
					 ,Particle *p
					 ,CTime &jikan){
  double free_energy[3];
  Calc_free_energy_PB(Concentration, p, free_energy, up[0], up[1], up[2], jikan);
  double free_energy_previous=free_energy[0];
  //  fprintf(stderr, "#1:time #2:total 3:ideal_gas 4:electrostatic 5:error\n");
  //  fprintf(stderr, "%d %.15g %.15g %.15g\n"
  //	  ,0, free_energy[0], free_energy[1], free_energy[2]);
  int STEP=10000;
  for(int m=1;m<STEP;m++){
    for(int d=0;d<DIM;d++){
      Reset_phi(u[d]);
    }
    ///////////////////////
    {
      double dmy_uk_dc[DIM];
      U_k2zeta_k(u, up, dmy_uk_dc);
      //for two-thirds rule 
      const Index_range ijk_range[] = {
	{0,TRN_X-1, 0,TRN_Y-1, 0,2*TRN_Z-1}
	,{0,TRN_X-1, NY-TRN_Y+1,NY-1,  0,2*TRN_Z-1}
	,{NX-TRN_X+1,NX-1,  0,TRN_Y-1, 0,2*TRN_Z-1}
	,{NX-TRN_X+1,NX-1,  NY-TRN_Y+1,NY-1,  0,2*TRN_Z-1}
      };
      const int n_ijk_range = sizeof(ijk_range)/sizeof(Index_range);
      Ion_diffusion_solver_Euler(up, jikan, dmy_uk_dc, Concentration, p, ijk_range, n_ijk_range);
    }
    ///////////////////////
    Calc_free_energy_PB(Concentration, p, free_energy, up[0], up[1], up[2], jikan);
    double dum=fabs(free_energy[0]-free_energy_previous);
    //    if( m % 10 == 0){
    //      fprintf(stderr, "%d %.15g %.15g %.15g %g\n"
    //	      ,m, free_energy[0], free_energy[1], free_energy[2], dum);
    //    }
    if(dum < TOL) break;
    free_energy_previous=free_energy[0];
  }
}

inline void Set_uniform_ion_charge_density_nosalt(double *Concentration
					    ,double *Total_solute
					    ,Particle *p){
  if(N_spec != 1){//N_spec = 1 $B$K$+$.$k$3$H$KCm0U(B
    fprintf(stderr,"invalid (number of ion spencies) = %d\n",N_spec);
    exit_job(EXIT_FAILURE);
  }
  Reset_phi(phi);
  Make_phi_particle(phi,p);
  
  double volume_phi = 0.;
  double dmy_phi;
#pragma omp parallel for schedule(dynamic, 1) reduction(+:volume_phi) private(dmy_phi)
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ;k++){
	dmy_phi = phi[(i*NY*NZ_)+(j*NZ_)+k];
	volume_phi += (1.-dmy_phi);
      }
    }
  }

  double Counterion_density = Counterion_number / (volume_phi * DX3);
#pragma omp parallel for schedule(dynamic, 1) private(Counterion_density)
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ;k++){
	Concentration[(i*NY*NZ_)+(j*NZ_)+k] = Counterion_density;
      }
    }
  }

  Total_solute[0] = Count_single_solute(Concentration, phi);
  A2a_k(Concentration);
}

inline void Set_Poisson_Boltzmann_ion_charge_density_nosalt(double **Concentration
						      ,double *Total_solute
						      ,Particle *p){
  if(N_spec != 1){//N_spec = 1 $B$K$+$.$k$3$H$KCm0U(B
    fprintf(stderr,"invalid (number of ion spencies) = %d\n",N_spec);
    exit_job(EXIT_FAILURE);
  }
  
  Reset_phi(phi);
  Make_phi_particle(phi, p);
  
  int STEP=100000;
  double alp=0.01;//0.001;//0.1;
  
  double dmy1 = Valency[0] * Elementary_charge / kBT;
  double *e_potential = up[2];
  for(int m=1;m<STEP;m++){
    {
      Conc_k2charge_field(p, Concentration, e_potential, up[0], up[1]);
      A2a_k(e_potential);
      Charge_field_k2Coulomb_potential_k_PBC(e_potential);
    }
    A_k2a(e_potential);
    
    double rho_0 = 0.;
    double dmy_phi;
#pragma omp parallel for schedule(dynamic, 1) reduction(+:rho_0) private(dmy_phi)
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
 	  for(int k=0;k<NZ;k++){
 	  dmy_phi = phi[(i*NY*NZ_)+(j*NZ_)+k];
 	  rho_0 += (1.-dmy_phi) * exp( - dmy1 * e_potential[(i*NY*NZ_)+(j*NZ_)+k] );
 	}
      }
    }
    rho_0 = Counterion_number / (rho_0 * DX3);
    A_k2a(Concentration[0]);
    
    double sum=0.;
    double error=0.;
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
 	  for(int k=0;k<NZ;k++){
 	  double dmy = rho_0 * exp ( - dmy1 * e_potential[(i*NY*NZ_)+(j*NZ_)+k] );
 	  Concentration[0][(i*NY*NZ_)+(j*NZ_)+k] = (1.-alp) * Concentration[0][(i*NY*NZ_)+(j*NZ_)+k] + alp * dmy;
 	  sum=sum+fabs(dmy);
 	  error=error+fabs(Concentration[0][(i*NY*NZ_)+(j*NZ_)+k]-dmy);
 	}
      }
	}
    A2a_k(Concentration[0]);
    {
      double *rescale_factor = new double[N_spec];
      Rescale_solute(rescale_factor
		     ,Total_solute
		     ,Concentration, p, up[0], up[1]);
      delete [] rescale_factor;
    }
    if(error/sum < TOL) break;
  }
  
  A_k2a_out(Concentration[0], up[0]);
  Total_solute[0] = Count_single_solute(up[0], phi);
}
inline void Set_uniform_ion_charge_density_salt(double **Concentration
					  ,double *Total_solute
					  ,Particle *p){
  if(N_spec != 2){//N_spec = 2 $B$K$+$.$k$3$H$KCm0U(B
    fprintf(stderr,"invalid (number of ion spencies) = %d\n",N_spec);
    exit_job(EXIT_FAILURE);
  }

  Reset_phi(phi);
  Make_phi_particle(phi, p);

  double positive_ion_number=0.;
  double negative_ion_number=0.;
  for(int i=0;i<Component_Number;i++){
    double dmy_surface_charge = Surface_charge[i];
    double dmy_particle_number = (double)Particle_Numbers[i];
    if(dmy_surface_charge < 0.){
      positive_ion_number += fabs(dmy_surface_charge * dmy_particle_number / Valency[0]);
    }else if(dmy_surface_charge > 0.){
      negative_ion_number += fabs(dmy_surface_charge * dmy_particle_number / Valency[1]);
    }
  }
  double volume_phi = 0.;
  double dmy_phi;
  int im;
#pragma omp parallel for schedule(dynamic, 1) reduction(+:volume_phi) private(dmy_phi,im)
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ;k++){
    im=(i*NY*NZ_)+(j*NZ_)+k;
	dmy_phi = phi[im];
	volume_phi += (1.-dmy_phi);
      }
    }
  }
  double positive_ion_density = positive_ion_number / (volume_phi * DX3);
  double negative_ion_density = negative_ion_number / (volume_phi * DX3);
  
  double Rho_inf = kBT * Dielectric_cst / SQ(Elementary_charge * Debye_length);
  double Rho_inf_positive_ion=Rho_inf/(Valency[0]*(Valency[0]-Valency[1]));
  double Rho_inf_negative_ion=-Rho_inf/(Valency[1]*(Valency[0]-Valency[1]));

#pragma omp parallel for schedule(dynamic, 1) 
  for(int i=0;i<NX;i++){
    for(int j=0;j<NY;j++){
      for(int k=0;k<NZ;k++){
		int im=(i*NY*NZ_)+(j*NZ_)+k;
	Concentration[0][im] = positive_ion_density + Rho_inf_positive_ion;
	Concentration[1][im] = negative_ion_density + Rho_inf_negative_ion;
      }
    }
  }
  Total_solute[0] = Count_single_solute(Concentration[0], phi);
  Total_solute[1] = Count_single_solute(Concentration[1], phi);

  A2a_k(Concentration[0]);
  A2a_k(Concentration[1]);
  
  fprintf(stderr,"# density of positive and negative ions %g %g\n", positive_ion_density + Rho_inf_positive_ion,negative_ion_density + Rho_inf_negative_ion);
}

inline void Set_Poisson_Boltzmann_ion_charge_density_salt(double **Concentration
						    ,double *Total_solute
						    ,Particle *p){
  if(N_spec != 2){//N_spec = 2 $B$K$+$.$k$3$H$KCm0U(B
    fprintf(stderr,"invalid (number of ion spencies) = %d\n",N_spec);
    exit_job(EXIT_FAILURE);
  }
  if(Valency[0] != -Valency[1]){//$BBP>N(Bz:z$BEE2r<AMO1U(B Valency_positive_ion=z, Valency_negative_ion=-z $B$N>l9g$K$+$.$k(B
    fprintf(stderr,"invalid (Valency_positive_ion, Valency_negative_ion) = %g %g\n",Valency[0],Valency[1]);
    fprintf(stderr,"set Valency_positive_ion = - Valency_negative_ion\n");
    exit_job(EXIT_FAILURE);
  }

  double Rho_inf = kBT * Dielectric_cst / SQ(Elementary_charge * Debye_length);
  double Rho_inf_positive_ion=Rho_inf/(Valency[0]*(Valency[0]-Valency[1]));
  double Rho_inf_negative_ion=-Rho_inf/(Valency[1]*(Valency[0]-Valency[1]));
  //$B;E9~$_$N%$%*%sG;EY(B($B%P%k%/$G%+%&%s%?!<%$%*%s(B, $B6&%$%*%s$OG;EY(BRho_inf_positive_ion, Rho_inf_negative_ion$B$GJ?9U$G$"$k$H2>Dj(B)
  fprintf(stderr,"# bulk density of positive and negative ions  %g %g\n", Rho_inf_positive_ion, Rho_inf_negative_ion);
  
  int STEP=100000;
  double alp=0.01;//0.001;//0.1;
  double dmy0 = Elementary_charge * Valency[0] / kBT;
  double dmy1 = Elementary_charge * Valency[1] / kBT;
  
  Reset_phi(phi);
  Make_phi_particle(phi, p);
  double *e_potential0 = up[0];
  double *e_potential1 = up[1];
  double *dmy_value0 = f_particle[0];
  double *dmy_value1 = f_particle[1];
  Reset_phi(e_potential0);
  
  for(int m=1;m<STEP;m++){
    double dmy_rho_positive_ion = 0.;
    double dmy_rho_negative_ion = 0.;
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
 	for(int k=0;k<NZ;k++){
		int im=(i*NY*NZ_)+(j*NZ_)+k;
 	  double dmy_phi = phi[im];
 	  dmy_rho_positive_ion += (1.-dmy_phi) * exp( - dmy0 * e_potential0[im] );
 	  dmy_rho_negative_ion += (1.-dmy_phi) * exp( - dmy1 * e_potential0[im] );
 	}
      }
    }

    dmy_rho_positive_ion *= DX3;
    dmy_rho_negative_ion *= DX3;
    
    double a=Valency[0]*Rho_inf_positive_ion*dmy_rho_positive_ion;
    double b=-Surface_ion_number;
    double c=Valency[1]*Rho_inf_negative_ion*dmy_rho_negative_ion;
    double nu = (-b+sqrt(b*b-4.*a*c))/(2.*a);

    double sum;
    double error;
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	for(int k=0;k<NZ;k++){
		int im=(i*NY*NZ_)+(j*NZ_)+k;
	  double dmy_pot = e_potential0[im];
	  Concentration[0][im] = Rho_inf_positive_ion * exp( -dmy0 * dmy_pot ) * nu;
	  Concentration[1][im] = Rho_inf_negative_ion * exp( -dmy1 * dmy_pot ) / nu;
	  e_potential1[im] = dmy_pot;
	}
      }
    }
    {
      Reset_phi(dmy_value0);
      Reset_phi(dmy_value1);
      Make_phi_qq_particle(dmy_value0, dmy_value1, p);
      for(int n=0;n<N_spec;n++){
	for(int i=0;i<NX;i++){
	  for(int j=0;j<NY;j++){
	    for(int k=0;k<NZ;k++){
		int im=(i*NY*NZ_)+(j*NZ_)+k;
	      double dmy_phi = dmy_value0[im];
	      double dmy_conc = Concentration[n][im];
	      dmy_value1[im] += Valency_e[n] * dmy_conc * (1.- dmy_phi);
	    }
	  }
	}
      }
    }

    if(0){
    }else {
      A2a_k(dmy_value1);
      Charge_field_k2Coulomb_potential_k_PBC(dmy_value1);
      A_k2a(dmy_value1);
      
      sum=0.;
      error=0.;
      for(int i=0;i<NX;i++){
	for(int j=0;j<NY;j++){
	  for(int k=0;k<NZ;k++){
		int im=(i*NY*NZ_)+(j*NZ_)+k;
	    double dmy = alp * (dmy_value1[im] - e_potential0[im]);
	    e_potential0[im] += dmy;
	    sum += fabs(e_potential0[im]);
	    error += fabs(dmy);
	  }
	}
      }
      //      fprintf(stderr,"# check the convergence  %d %g\n", m,error/sum);
    }
    if(error/sum < TOL) break;
  }
  Total_solute[0] = Count_single_solute(Concentration[0], phi);
  Total_solute[1] = Count_single_solute(Concentration[1], phi);

  A2a_k(Concentration[0]);
  A2a_k(Concentration[1]);
}

void Init_rho_ion(double **Concentration, Particle *p, CTime &jikan){

  for(int i=0;i<Component_Number;i++){//$B%3%m%$%II=LLEE2Y$O(B0$B0J30$K@_Dj$9$k(B
    if(Surface_charge[i] == 0.){
      fprintf(stderr,"invalid (Surface_charge) = %g\n",Surface_charge[i]);
      exit_job(EXIT_FAILURE);
    }
    Surface_charge_e[i] = Surface_charge[i] * Elementary_charge;
  }

  Bjerrum_length = SQ(Elementary_charge)/(PI4 * kBT * Dielectric_cst);
  Surface_ion_number = 0.;
  for(int i=0; i<Component_Number; i++){
    Surface_ion_number -= Surface_charge[i] * Particle_Numbers[i];
  }

  if(N_spec == 1){
    Valency[0] = Valency_counterion;
    Onsager_coeff[0]= Onsager_coeff_counterion;
    Counterion_number = Surface_ion_number / Valency_counterion;
  }else if(N_spec == 2){
    Valency[0] = Valency_positive_ion;
    Valency[1] = Valency_negative_ion;
    Onsager_coeff[0]= Onsager_coeff_positive_ion;
    Onsager_coeff[1]= Onsager_coeff_negative_ion;
  }

  for(int n=0;n<N_spec;n++){
    Valency_e[n] = Valency[n] * Elementary_charge;
  }

  if(N_spec == 1){
    if(Component_Number == 1){
      if(Valency_counterion * Surface_charge[0] >= 0.){//$B%$%*%s2A?t$O%3%m%$%II=LLEE2Y$H$O0[Id9f$K$9$k(B
	fprintf(stderr,"invalid (Valency_counterion) = %g\n",Valency_counterion);
	exit_job(EXIT_FAILURE);
      }
    }else{
      double dmy0 = Surface_charge[0];
      for(int i=1; i<Component_Number; i++){
	double dmy1 = Surface_charge[i];
 	if(dmy0 * dmy1 <= 0.){//saltfree$B$N$H$-@5Ii%3%m%$%I:.9g7O$K$OL$BP1~(B
	  fprintf(stderr,"invalid (number of ion spencies) = %d\n",N_spec);
	  fprintf(stderr,"select salt in Add_salt\n");
	  exit_job(EXIT_FAILURE);
	}
      }
    }

    //Reset_phi(phi);
    Reset_phi(phi);
    //Make_phi_particle(phi, p);
    Make_phi_particle(phi, p);
    double volume_phi = 0.;
    for(int i=0;i<NX;i++){
      for(int j=0;j<NY;j++){
	for(int k=0;k<NZ;k++){
	  //double dmy_phi = phi[i][j][k];
	  double dmy_phi = phi[(i*NY*NZ_)+(j*NZ_)+k];
	  volume_phi += (1.-dmy_phi);
	}
      }
    }

    double Counterion_density = Counterion_number / (volume_phi * DX3);
    fprintf(stderr,"############################ initial state for counterions\n");
    fprintf(stderr,"# counterion_density %g\n", Counterion_density);
    fprintf(stderr,"# Debye length (1/(4 pi Bjerrum_length rho_c)^{1/2}) %g\n"
	    ,1./sqrt(PI4 * SQ(Valency[0]) * Bjerrum_length * Counterion_density));
    fprintf(stderr,"# radius of particle / Debye length  %g\n"
	    ,RADIUS * sqrt(PI4 * SQ(Valency[0]) * Bjerrum_length * Counterion_density));

    if(Poisson_Boltzmann){
      Set_uniform_ion_charge_density_nosalt(Concentration[0], Total_solute, p);
      Set_Poisson_Boltzmann_ion_charge_density_nosalt(Concentration, Total_solute, p);
      fprintf(stderr,"# initialized by Poisson-Boltzmann distribution\n");
      if(External_field){
	Set_steadystate_ion_density(Concentration, p, jikan);
	fprintf(stderr,"# initialized by Poisson-Boltzmann distribution under external field\n");
      }
    }else{
      Set_uniform_ion_charge_density_nosalt(Concentration[0], Total_solute, p);
      fprintf(stderr,"# initialized by uniform distribution\n");
    }
    fprintf(stderr,"############################\n");
  }else{
    if(Valency_positive_ion <= 0.){//$B@5%$%*%s2A?t(B > 0
      fprintf(stderr,"invalid (Valency_positive_ion) = %g\n",Valency_positive_ion);
      exit_job(EXIT_FAILURE);
    }
    if(Valency_negative_ion >= 0.){//$BIi%$%*%s2A?t(B < 0
      fprintf(stderr,"invalid (Valency_negative_ion) = %g\n",Valency_negative_ion);
      exit_job(EXIT_FAILURE);
    }
    fprintf(stderr,"############################ initial state for positive and negative ions\n");
    fprintf(stderr,"# radius of particle / Debye length  %g\n" ,RADIUS / Debye_length);
    if(Poisson_Boltzmann){
      if(External_field){
	if(Particle_Number == 1){
	  Set_Poisson_Boltzmann_ion_charge_density_salt(Concentration, Total_solute, p);
	  fprintf(stderr,"# initialized by Poisson-Boltzmann distribution\n");
	}else {
	  Set_uniform_ion_charge_density_salt(Concentration, Total_solute, p);
	  Set_steadystate_ion_density(Concentration, p, jikan);
	  fprintf(stderr,"# initialized by Poisson-Boltzmann distribution under external field\n");
	}
      }else{
	if(Particle_Number == 1){
	  Set_Poisson_Boltzmann_ion_charge_density_salt(Concentration, Total_solute, p);
	  fprintf(stderr,"# initialized by Poisson-Boltzmann distribution\n");
	}else {
	  Set_uniform_ion_charge_density_salt(Concentration, Total_solute, p);
	  Set_steadystate_ion_density(Concentration, p, jikan);
	  fprintf(stderr,"# initialized by Poisson-Boltzmann distribution\n");
	}
      }
    }else{
      Set_uniform_ion_charge_density_salt(Concentration, Total_solute, p);
      fprintf(stderr,"# initialized by uniform distribution\n");
    }
    fprintf(stderr,"############################\n");
  }
  Count_solute_each(Total_solute, Concentration, p, phi, up[0]);
}

void Mem_alloc_charge(void){

  Valency       = alloc_1d_double(N_spec);
  Onsager_coeff = alloc_1d_double(N_spec);
  Mem_alloc_solute();

}
