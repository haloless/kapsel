//
// $Id: f_particle.h,v 1.7 2005/07/27 09:45:53 nakayama Exp $
//
#ifndef F_PARTICLE_H
#define F_PARTICLE_H

#include "variable.h"
#include "operate_omega.h"
#include "input.h"
#include "make_phi.h"

extern Value *f_particle;
void Mem_alloc_f_particle(void);

inline void Make_f_particle_dt_nonsole(Value f[DIM]
				       ,const Value u[DIM]
				       ,const Value up[DIM]
				       ,const Value &phi
				       ){
    // !! $B$3$l$r8F$VA0$K!"(B up, phi $B$r7W;;$7$F$*$/$3$H!#(B
    for(int d=0; d<DIM; d++){
	for(int i=0; i<NX; i++){
	    for(int j=0; j<NY; j++){
		for(int k=0; k<NZ; k++){
		    f[d][i][j][k] = up[d][i][j][k] - phi[i][j][k] * u[d][i][j][k];
		}
	    }
	}	
    }
    if(SW_EQ == Shear_Navier_Stokes){
	Symmetrize_u(f);
    }
}
inline void Make_f_particle_dt_sole(Value f[DIM]
				    ,const Value u[DIM]
				    ,const Value up[DIM]
				    ,const Value &phi
				    ){
    // !! $B$3$l$r8F$VA0$K!"(B up, phi $B$r7W;;$7$F$*$/$3$H!#(B
    Make_f_particle_dt_nonsole(f, u, up, phi);
    {
      Solenoidal_u(f);
    }
}

inline void Add_f_particle(Value *u, const Value *f, const int dim=DIM){
//inline void Add_f_particle(Value u[DIM], const Value f[DIM]){
    // !! $B$3$l$r8F$VA0$K!"(B f $B$r7W;;$7$F$*$/$3$H!#(B
    for(int d=0; d<dim; d++){
	for(int i=0; i<NX; i++){
	    for(int j=0; j<NY; j++){
		for(int k=0; k<NZ; k++){
		    u[d][i][j][k] += f[d][i][j][k];
		}
	    }
	}	
    }
}

#endif
