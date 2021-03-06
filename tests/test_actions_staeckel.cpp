/** \file    test_actions_staeckel.cpp
    \author  Eugene Vasiliev
    \date    2015-2016

    This test verifies the correctness of action/angle determination by the Staeckel fudge.

    We create an instance of Staeckel potential (in this example, oblate perfect ellipsoid),
    perform numerical orbit integration in this potential, and for each point
    on the trajectory, compute the values of actions and angles.
    Two related methods are employed - the first is valid only for a Staeckel potential,
    and the second (fudge) is an approximation which essentially turns out to be exact
    for this potential.
    We test that the values of actions are nearly constant (to the limit of orbit
    integration accuracy), and that angles increase linearly with time.
    We use several initial conditions corresponding to various generic and extreme cases
    (e.g., an in-plane orbit, or an orbit with very small radial action, or an orbit
    close to the separatrix between box and tube orbits in x-z plane).
*/
#include "potential_perfect_ellipsoid.h"
#include "actions_staeckel.h"
#include "orbit.h"
#include "math_core.h"
#include "debug_utils.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

const double eps=5e-8;               // accuracy of comparison
const double epsi=5e-7;              // accuracy of comparison for interpolator
const double epsint=3e-3;            // accuracy of action interpolator
const double axis_a=1.6, axis_c=1.0; // axes of perfect ellipsoid
const bool output=utils::verbosityLevel >= utils::VL_VERBOSE;  // whether to create text files with orbits
const char* err=" \033[1;31m**\033[0m\n";
const char* exc=" \033[1;33mCAUGHT EXCEPTION\033[0m\n";

template<typename CoordT>
bool test_oblate_staeckel(const potential::OblatePerfectEllipsoid& potential,
    const actions::ActionFinderAxisymFudge& actfinder,
    const coord::PosVelT<CoordT>& initial_conditions,
    const double total_time, const double timestep)
{
    std::vector<std::pair<coord::PosVelT<CoordT>, double> > traj = orbit::integrateTraj(
        initial_conditions, total_time, timestep, potential);
    actions::ActionStat stats, statf, stati;
    actions::Angles angf;
    bool exs=false, exf=false, exi=false;
    std::ofstream strm;
    if(output) {
        std::ostringstream s;
        double x[6];
        initial_conditions.unpack_to(x);
        s<<"ActionsStaeckel_"<<CoordT::name()<<'_'<<
            x[0]<<'_'<<x[1]<<'_'<<x[2]<<'_'<<x[3]<<'_'<<x[4]<<'_'<<x[5];
        strm.open(s.str().c_str());
    }
    // two estimates of focal distance: from the trajectory
    // (sensible unless z==0 everywhere, in which case its value doesn't matter anyway)
    // and from the interpolator
    std::vector<coord::PosT<CoordT> > trajpoints(traj.size());
    for(size_t i=0; i<traj.size(); i++)
        trajpoints[i] = traj[i].first;
    double ifd_p = actions::estimateFocalDistancePoints(potential, trajpoints);
    double ifd_i = actfinder.focalDistance(toPosVelCyl(initial_conditions));
    for(size_t i=0; i<traj.size(); i++) {
        const coord::PosVelCyl pc = coord::toPosVelCyl(traj[i].first);
        try {
            actions::ActionAngles a = actions::actionAnglesAxisymStaeckel(potential, pc);
            stats.add(a);
        }
        catch(std::exception &e) {
            if(!exs) std::cout << "Exception in Staeckel at i="<<i<<": "<<e.what()<<"\n";
            exs=true;
        }
        try {
            actions::ActionAngles a = actions::actionAnglesAxisymFudge(potential, pc, ifd_p);
            statf.add(a);
            if(1 || i==0) angf=a;  // 1 to disable unwrapping
            else {
                angf.thetar   = math::unwrapAngle(a.thetar, angf.thetar);
                angf.thetaz   = math::unwrapAngle(a.thetaz, angf.thetaz);
                angf.thetaphi = math::unwrapAngle(a.thetaphi, angf.thetaphi);
            }
            if(output) {
                const coord::PosVelProlSph pp = coord::toPosVel<coord::Cyl,coord::ProlSph>
                    (pc, potential.coordsys());
                strm << i*timestep<<"   "<<
                    pc.phi<<" "<<pc.vphi<<" "<<pp.lambda<<" "<<pp.nu<<" "<<
                    pp.lambdadot<<" "<<pp.nudot<<"  "<<
                    angf.thetar<<" "<<angf.thetaz<<" "<<angf.thetaphi<<"  "<<
                    utils::pp(a.Jr,12)<<" "<<utils::pp(a.Jz,12)<<"  ";
            }
        }
        catch(std::exception &e) {
            if(!exf) std::cout << "Exception in Fudge at i="<<i<<": "<<e.what()<<"\n";
            exf=true;
        }
        try {
            actions::Actions a = actfinder.actions(pc);
            stati.add(a);
            if(output)
                strm << utils::pp(a.Jr,12)<<" "<<utils::pp(a.Jz,12)<<"\n";
        }
        catch(std::exception &e) {
            if(!exi) std::cout << "Exception in Interpolator at i="<<i<<": "<<e.what()<<"\n";
            exi=true;
        }
    }
    stats.finish();
    statf.finish();
    stati.finish();
    bool oks  = stats.rms.Jr<eps && stats.rms.Jz<eps && stats.rms.Jphi<eps && !exs;
    bool okf  = statf.rms.Jr<eps && statf.rms.Jz<eps && statf.rms.Jphi<eps && !exf
        && fabs(stats.avg.Jr-statf.avg.Jr)<eps
        && fabs(stats.avg.Jz-statf.avg.Jz)<eps
        && fabs(stats.avg.Jphi-statf.avg.Jphi)<eps
        &&     (stats.avg.Jz==0 || fabs(ifd_p - ifd_i)<1e-5);
    bool oki  = stati.rms.Jr<epsi&& stati.rms.Jz<epsi&& stati.rms.Jphi<eps && !exi
        && fabs(stats.avg.Jr-stati.avg.Jr)<epsint
        && fabs(stats.avg.Jz-stati.avg.Jz)<epsint;
    std::string csname(CoordT::name());
    csname += ',';
    csname.resize(12, ' ');
    std::cout << csname << " Exact"
    ":  Jr="  << utils::pp(stats.avg.Jr,  9) <<" +- "<< utils::pp(stats.rms.Jr,  7) <<
    ",  Jz="  << utils::pp(stats.avg.Jz,  9) <<" +- "<< utils::pp(stats.rms.Jz,  7) <<
    ",  Jphi="<< utils::pp(stats.avg.Jphi,9) <<" +- "<< utils::pp(stats.rms.Jphi,7) <<
    (exs ? exc : !oks ? err : "\n");
    std::cout << csname << " Fudge"
    ":  Jr="  << utils::pp(statf.avg.Jr,  9) <<" +- "<< utils::pp(statf.rms.Jr,  7) <<
    ",  Jz="  << utils::pp(statf.avg.Jz,  9) <<" +- "<< utils::pp(statf.rms.Jz,  7) <<
    ",  Jphi="<< utils::pp(statf.avg.Jphi,9) <<" +- "<< utils::pp(statf.rms.Jphi,7) <<
    (exf ? exc : !okf ? err : "\n");
    std::cout << csname << " Inter"
    ":  Jr="  << utils::pp(stati.avg.Jr,  9) <<" +- "<< utils::pp(stati.rms.Jr,  7) <<
    ",  Jz="  << utils::pp(stati.avg.Jz,  9) <<" +- "<< utils::pp(stati.rms.Jz,  7) <<
    ",  Jphi="<< utils::pp(stati.avg.Jphi,9) <<" +- "<< utils::pp(stati.rms.Jphi,7) <<
    (exi ? exc : !oki ? err : "\n");
    return oks && okf && oki;
}

bool test_three_cs(const potential::PtrOblatePerfectEllipsoid& potential,
    const actions::ActionFinderAxisymFudge& actfinder,
    const coord::PosVelCar& initcond, const char* title)
{
    const double total_time=100.;
    const double timestep=1./8;
    bool ok=true;
    std::cout << "\033[1;37m   ===== "<<title<<" =====\033[0m\n";
    ok &= test_oblate_staeckel(*potential, actfinder, coord::toPosVelCar(initcond), total_time, timestep);
    ok &= test_oblate_staeckel(*potential, actfinder, coord::toPosVelCyl(initcond), total_time, timestep);
    ok &= test_oblate_staeckel(*potential, actfinder, coord::toPosVelSph(initcond), total_time, timestep);
    return ok;
}

int main() {
    potential::PtrOblatePerfectEllipsoid pot(new potential::OblatePerfectEllipsoid(1.0, axis_a, axis_c));
    const actions::ActionFinderAxisymFudge af(pot, true);
    bool allok=true;
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0.3, 0.1, 0.1, 0.4, 0.1   ), "ordinary case");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0  , 0.2, 0.3486), "thin orbit (Jr~0)");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0. , 0. , 0.4732), "thin orbit in x-z plane (Jr~0, Jphi=0)");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0. , 0. , 0.4097), "tube orbit in x-z plane near separatrix");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0. , 0. , 0.4096), "box orbit in x-z plane near separatrix");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0. ,1e-8, 0.4097), "orbit with Jphi<<J_z");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0.3, 0. , 0.1, 0.4, 1e-4  ), "almost in-plane orbit (Jz~0)");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0.3, 0. , 0.1, 0.4, 0.    ), "exactly in-plane orbit (Jz=0)");
    allok &= test_three_cs(pot, af, coord::PosVelCar(1, 0. , 0. , 0. ,.296, 0.    ), "almost circular in-plane orbit (Jz=0,Jr~0)");
    if(allok)
        std::cout << "\033[1;32mALL TESTS PASSED\033[0m\n";
    else
        std::cout << "\033[1;31mSOME TESTS FAILED\033[0m\n";
    return 0;
}