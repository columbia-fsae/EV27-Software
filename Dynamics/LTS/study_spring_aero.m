%% study_spring_aero.m
% Demonstrate the SPRING <-> AERO trade-off for an FSAE car using a measured
% CLA-vs-ride-height map, on top of the existing quasi-steady lapsim.
%
% Spring rate couples to lap time through TWO channels with OPPOSITE sign:
%   (A) Mechanical grip : softer springs -> less contact-patch-load variation
%       (sigma_Fz) -> less load-sensitivity loss on muy -> scales factor_y.
%       (mux() is load-independent in this tyre model -> mechanical CPL effect
%        is LATERAL ONLY.)
%   (B) Aero platform   : the CLA(RH) map PEAKS at RH~1 with a stall cliff below.
%       Stiffer springs -> less aero squat AND less ride-height variation -> the
%       platform sits closer to the peak / averages less into the stall side ->
%       higher effective CLA -> injected via car.aero.cla.
%
% Lapsim is point-mass / quasi-steady (no ride-height state) -> both effects are
% computed HERE per spring setting and injected as scalars.
%
% Requires: cars.m tracks.m lapsim.m compete.m lap2score.m Tire.m
%           R20_tire_model_updated.mat
% ----------------------------------------------------------------------------
clear; clc; close all;
cars; tracks;
baseline = ev26b;
tire     = baseline.tires.model;
g   = 9.806;  rho = autox_event.air_density;
fy0 = baseline.tires.factor_y;

%% ============================ USER INPUTS ===================================
% ---- Measured CLA-vs-ride-height map (your data). Lift is negative in CFD;
%      we flip sign so the lapsim sees a POSITIVE downforce coefficient. ----
RH_tab  = [0.5   1.0   1.5    2.0   2.5   3.0 ];   % ride height (native units)
ClA_tab = [-2.557 -3.11 -2.935 -2.87 -2.78 -2.67]; % CFD ClA (negative = downforce)
RH_unit_mm = 25.4;     % <-- CONFIRM map RH units: inch=25.4, cm=10, mm=1

% shape-preserving interpolant, clamped to the table ends (no wild extrapolation)
cla_of_rh = @(RH) interp1(RH_tab, -ClA_tab, ...
                  min(max(RH,RH_tab(1)),RH_tab(end)), 'pchip');

% ---- Static ride-height setup (native units). Set so the OPERATING height
%      (static minus aero squat) lands near the map peak (~1). ----
RH_static = 1.1;

% ---- Spring sweep: per-corner WHEEL rates [N/m], both axles scaled together
kw_f0 = 30e3;  kw_r0 = 30e3;            
scale = linspace(0.5, 2.3, 22);

% ---- Vertical tyre / unsprung
k_t  = 110e3;  m_u = 13;  zeta = 0.30; % <-- CONFIRM k_t, m_u

% ---- Operating point for MEAN corner loads (where on muy(FZ) you sit)
Ay_op = 1.4;  v_op = 13;

% ---- Road spectrum (ISO 8608); autocross asphalt ~ class A-B
road = struct('Gd_n0',2e-6,'n0',0.1,'w',2.0,'lo',0.02,'hi',12,'N',3000,'V',v_op);

RUN_FULL_COMPETE = false;
%% ===========================================================================

% Geometry / static split
l = baseline.l; t = baseline.t; cgh = baseline.cg(3); flltd = baseline.flltd;
wbf = 1 - baseline.cg(1)/l;  W = baseline.mass*g;
sf = W*wbf/2;  sr = W*(1-wbf)/2;
m_s_f = sf/g - m_u;  m_s_r = sr/g - m_u;

% Representative downforce from the map at the target height (real ~3.1, not 0.51)
cla_pk = cla_of_rh(RH_static);
DF0    = 0.5*rho*cla_pk*v_op^2;
aerof  = 1 - baseline.cp(1)/l;
af = DF0*aerof/2;  ar = DF0*(1-aerof)/2;
dWt = baseline.mass*Ay_op*g*cgh/t;  dWf = flltd*dWt;  dWr = (1-flltd)*dWt;
Fz_corner = max([sf+af-dWf, sf+af+dWf, sr+ar-dWr, sr+ar+dWr], 1);
wgt = Fz_corner/sum(Fz_corner);

% Reference CLA for the mech-only decomposition (hold aero fixed)
cla_ref = cla_of_rh(RH_static);

%% --------------------------- Sweep --------------------------------------
nS = numel(scale);
[cla_eff,g_mech,sigRH_mm,RH_op,fride] = deal(zeros(nS,1));
[t_comb,t_aero,t_mech] = deal(zeros(nS,1));
score_comb = nan(nS,1);

for s = 1:nS
    kw_f = kw_f0*scale(s);  kw_r = kw_r0*scale(s);
    rr_f = kw_f*k_t/(kw_f+k_t);  rr_r = kw_r*k_t/(kw_r+k_t);
    K_heave = 2*rr_f + 2*rr_r;
    fride(s) = 1/(2*pi)*sqrt(rr_f/m_s_f);

    [sFz_f,sRH_f] = qcar(kw_f,m_s_f,m_u,k_t,zeta,road);
    [sFz_r,~    ] = qcar(kw_r,m_s_r,m_u,k_t,zeta,road);
    sigFz = [sFz_f sFz_f sFz_r sFz_r];
    sigRH_mm(s) = sRH_f*1000;

    % ---- AERO channel: squat + variation around the map peak ----
    squat_nat = (DF0/K_heave)*1000 / RH_unit_mm;   % aero squat in native units
    sigRH_nat = sigRH_mm(s) / RH_unit_mm;
    RH_op(s)  = RH_static - squat_nat;             % operating height
    cla_eff(s)= eff_avg(cla_of_rh, RH_op(s), sigRH_nat);

    % ---- MECH channel: load-weighted lateral grip ratio (R20 muy) ----
    ratio = zeros(1,4);
    for c = 1:4
        cap = @(Fz) Fz .* tire.muy(-Fz);
        ratio(c) = eff_avg(cap, Fz_corner(c), sigFz(c)) / cap(Fz_corner(c));
    end
    g_mech(s) = ratio*wgt(:);

    % ---- Inject three ways ----
    car = baseline; car.aero.cla = cla_eff(s); car.tires.factor_y = fy0*g_mech(s);
    t_comb(s) = lapsim(autox_event,car).time;
    car = baseline; car.aero.cla = cla_eff(s);
    t_aero(s) = lapsim(autox_event,car).time;
    car = baseline; car.aero.cla = cla_ref; car.tires.factor_y = fy0*g_mech(s);
    t_mech(s) = lapsim(autox_event,car).time;

    if RUN_FULL_COMPETE
        car = baseline; car.aero.cla = cla_eff(s); car.tires.factor_y = fy0*g_mech(s);
        score_comb(s) = compete(car).score;
    end
    fprintf('scale %.2f | f_ride %.2f Hz | RH_op %.2f | cla_eff %.3f | g_mech %.3f | autox %.3fs\n',...
            scale(s),fride(s),RH_op(s),cla_eff(s),g_mech(s),t_comb(s));
end

[~,iopt] = min(t_comb);
fprintf('\nOptimum: scale %.2f (f_ride %.2f Hz, RH_op %.2f) -> autox %.3fs\n',...
        scale(iopt),fride(iopt),RH_op(iopt),t_comb(iopt));

%% --------------------------- Plots --------------------------------------

% subplot(1,3,1); hold on; grid on;       % the map + operating band
% RHq = linspace(RH_tab(1),RH_tab(end),200);
% plot(RHq, arrayfun(cla_of_rh,RHq),'k-','LineWidth',1.6);
% plot(RH_tab,-ClA_tab,'ko','MarkerFaceColor','k');
% plot(RH_op(iopt),cla_eff(iopt),'rp','MarkerFaceColor','r','MarkerSize',12);
% xlabel('ride height (native)'); ylabel('CLA (downforce +)');
% title('Your map + optimum operating pt');
% 
% subplot(1,3,2); hold on; grid on;       % the two channels
% yyaxis left;  plot(fride,g_mech,'-o','LineWidth',1.6); ylabel('g_{mech} (lat grip mult)');
% yyaxis right; plot(fride,cla_eff,'-s','LineWidth',1.6); ylabel('effective CLA');
% xlabel('front ride frequency [Hz]'); title('Opposite slopes');
% legend('mech grip','aero CLA','Location','best');

figure(); hold on; grid on;       % the trade-off
% plot(fride,t_aero,'--','LineWidth',1.4);
% plot(fride,t_mech,':','LineWidth',1.4);
% fride([6 8 10 end]) = [];
% t_comb([6 8 10 end]) = [];
plot(fride,t_comb,'-','LineWidth',2.0);
% plot(fride(iopt),t_comb(iopt),'ro','MarkerFaceColor','r');
% text(fride(iopt),t_comb(iopt),sprintf('  opt %.2f Hz',fride(iopt)));
xlabel('Ride Frequency [Hz]'); ylabel('Autocross time [s]');
title('Mech.Grip-Aero Trade-Off');

%% ========================= FUNCTIONS =======================================
function [sig_Fz,sig_RH] = qcar(kw,m_s,m_u,k_t,zeta,road)
    k_ride = kw*k_t/(kw+k_t);  c = 2*zeta*sqrt(k_ride*m_s);
    n  = linspace(road.lo,road.hi,road.N);
    Gd = road.Gd_n0*(n/road.n0).^(-road.w);
    s  = 1i*2*pi*road.V*n;  A = kw + c*s;
    Den   = (m_u*s.^2 + A + k_t).*(m_s*s.^2 + A) - A.^2;
    Zu_Zr = k_t.*(m_s*s.^2 + A)./Den;
    H_Fz  = k_t.*(Zu_Zr - 1);
    H_RH  = (A.*k_t)./Den - 1;
    sig_Fz = sqrt(max(trapz(n,(abs(H_Fz).^2).*Gd),0));
    sig_RH = sqrt(max(trapz(n,(abs(H_RH).^2).*Gd),0));
end

function v = eff_avg(fun,mu,sigma)
    if sigma < 1e-9, v = fun(mu); return; end
    lo = max(mu-5*sigma,1e-6); hi = mu+5*sigma;
    x  = linspace(lo,hi,401);
    p  = exp(-0.5*((x-mu)/sigma).^2); p = p/trapz(x,p);
    v  = trapz(x, arrayfun(fun,x).*p);
end

function y = despike(x)
% remove one-sided upward quantization spikes -> local median (no toolbox)
    x = x(:); n = numel(x); y = x; w = 2;
    for i = 1:n
        lo = max(1,i-w); hi = min(n,i+w);
        m = median(x(lo:hi));
        if x(i) > m, y(i) = m; end   % spikes are upward only -> pull down
    end
end
