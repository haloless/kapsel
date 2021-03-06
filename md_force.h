/*!
  \file md_force.h
  \brief Routines to compute MD forces on particles (header file)
  \author Y. Nakayama
  \date 2006/06/27
  \version 1.1
 */
#ifndef MD_FORCE_H
#define MD_FORCE_H

#include <assert.h> 
#include "variable.h"
#include "input.h"
#include "interaction.h"
#include "make_phi.h"

extern double Min_rij;
extern double Max_force;
extern double *Hydro_force;
extern double *Hydro_force_new;
extern double *Hydro_force_new_u;
extern double *Hydro_force_new_p;
extern double *Hydro_force_new_v;
extern double *Hydro_force_new_w;

enum Particle_BC {
  PBC_particle
  ,Lees_Edwards
  ,Shear_hydro
};

void Add_f_gravity(Particle *p);

/*!
  \brief Compute slip constribution to the hydrodynamic force
  \details \f{align*}{
  \left[\int\df{s}\vec{F}_i^{sq}\right] &= 
  \int\vdf{x}\rho\phi_i\left(\vec{u}^{**} - \vec{u}^{*}\right) \\
  \left[\int\df{s}\vec{N}_i^{sq}\right] &= 
  \int\vdf{x}\rho\phi_i\vec{r}_i\times\left(\vec{u}^{**} - \vec{u}^{*}\right)
  \f}
  \param[in,out] p particle data
  \param[in] u change in fluid velocity field due to slip profile at interfaces
  \param[in] jikan time data
 */
void Calc_f_slip_correct_precision(Particle *p, double const* const* u, const CTime &jikan);

/*!
  \brief Compute hydrodynamic force/torque acting on particles
  \details \f{align*}{
  \left[\int\df{s}\vec{F}_i^H\right] &=  -
  \int\vdf{x}\rho\phi_i\left(\vec{u}_p^n - \vec{u}^{*}\right) \\
  \left[\int\df{s}\vec{N}_i^H\right] &= -
  \int\vdf{x}\rho\phi_i\vec{r}_i\times\left(\vec{u}_p^n - \vec{u}^{*}\right)
  \f}
  \param[in,out] p particle data
  \param[in] u current fluid velocity field
  \param[in] jikan time data
 */
void Calc_f_hydro_correct_precision(Particle *p, double const* const* u, const CTime &jikan);

/*!
  \brief Compute hydrodynamic force acting on particles in a system with oblique coordinates (Lees-Edwards PBC)
 */
void Calc_f_hydro_correct_precision_OBL(Particle *p, double const* const* u, const CTime &jikan);

double Calc_f_Lennard_Jones_shear_cap_primitive_lnk(Particle *p
				       ,void (*distance0_func)(const double *x1,const double *x2,double &r12,double *x12)
				      ,const double cap
				       );
double Calc_f_Lennard_Jones_shear_cap_primitive(Particle *p
				       ,void (*distance0_func)(const double *x1,const double *x2,double &r12,double *x12)
				      ,const double cap
				       );
inline void Calc_f_Lennard_Jones(Particle *p){
//  Calc_f_Lennard_Jones_shear_cap_primitive(p,Distance0,DBL_MAX);
    Calc_f_Lennard_Jones_shear_cap_primitive_lnk(p, Distance0, DBL_MAX);
}
inline double Calc_f_Lennard_Jones_OBL(Particle *p){
    return Calc_f_Lennard_Jones_shear_cap_primitive(p, Distance0_OBL, DBL_MAX);
}
inline double Calc_anharmonic_force_chain(Particle *p, 
                                          void (*distance0_func)(const double *x1, const double *x2,double &r12, double *x12)){
    double anharmonic_spring_cst=30.*EPSILON/SQ(SIGMA);
    const double R0=1.5*SIGMA;
    const double iR0=1./R0;
    const double iR02=SQ(iR0);
    
    int rest_Chain_Number[Component_Number];
    for (int i = 0; i < Component_Number; i++) {
	rest_Chain_Number[i] = Chain_Numbers[i];
    }
    
    int n = 0;
    double shear_stress = 0.0;
    while (n < Particle_Number - 1) {
	for (int i = 0; i < Component_Number; i++) {
	    if (rest_Chain_Number[i] > 0) {
		for (int j = 0; j < Beads_Numbers[i]; j++) {
		    if (j < Beads_Numbers[i] - 1) {
			int m = n + 1;
			double dmy_r1[DIM];
                        double dm_r1 = 0.0;
                        distance0_func(p[m].x, p[n].x, dm_r1, dmy_r1);
                        
			double dm1=1./(1. - SQ(dm_r1)*iR02);
			
			if(dm1 < 0.0){
			    fprintf(stderr,"### anharmonic error\n");
			}
			
			for(int d=0; d<DIM; d++){
			    double dmy=dm1*dmy_r1[d];
			    p[n].fr[d] += -anharmonic_spring_cst*dmy;
			    p[m].fr[d] += anharmonic_spring_cst*dmy;
			}
                        shear_stress +=
                          ((-anharmonic_spring_cst * dm1 * dmy_r1[0]) * (dmy_r1[1]));
		    }
		    n++;
		}
		rest_Chain_Number[i]--;
	    }
	}
    }
    return shear_stress;
}

/*!
  \brief Chain debug info for chains in shear flow (x-y plane)
 */
inline void rigid_chain_debug(Particle *p, const double &time, 
                              const int &rigidID=0){
  if(SW_PT == rigid){
    int pid = Rigid_Particle_Cumul[rigidID];
    fprintf(stdout, "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n"
            ,time
            ,-atan2(GRvecs[pid][1], -GRvecs[pid][0])
            ,p[pid].x[0]
            ,p[pid].x[1]
            ,xGs[rigidID][0]
            ,xGs[rigidID][1]
            ,p[pid].v[0]
            ,p[pid].v[1]
            ,velocityGs[rigidID][0]
            ,velocityGs[rigidID][1]
            ,p[pid].omega[2]
            ,omegaGs[rigidID][2]
            ,forceGs[rigidID][0]
            ,forceGs[rigidID][1]
            ,torqueGs[rigidID][2]
            );
  }
}
#endif
