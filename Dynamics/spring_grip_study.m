%% spring_grip_study.m
% Quantify the effect of SPRING RATE on mechanical grip (contact-patch-load
% variation) and lap time for an FSAE car. Balance and aero-balance are held
% fixed: aero (CLA, CoP) and lateral load transfer enter ONLY as vertical-load
% inputs that set each tyre's operating Fz. The spring (wheel rate) is the only
% swept lever, so all RELATIVE numbers are pure mechanical-grip deltas.
%
% Pipeline:
%   1. Extract muX(Fz), muY(Fz) peak-grip curves from brushTireForce (your model)
%   2. Build 4 corner MEAN loads from car/aero/LLT inputs (the operating point)
%   3. For each spring rate -> quarter-car transmissibility vs ISO-8608 road PSD
%      -> sigma_Fz (RMS dynamic tyre-load variation) per corner
%   4. Effective peak force = E[ Fmax(Fz) ] over the load distribution (quadrature)
%      -> effective muX_eff, muY_eff referenced to the mean load
%   5. Grip change -> lap-time delta (cornering-limited power law, configurable)
%
% Requires: init_globals.m, brushTireForce.m, TC_Model_Inputs.mat on the path.
% ------------------------------------------------------------------------------
% clear; clc; close all;
init_globals();                          % loads Pacejka globals from your .mat

%% ============================ USER INPUTS ====================================
% ---- Given (your numbers) ----
P.tw      = 1.2;        % track width [m]
P.w_bias  = 0.5;        % front weight fraction (0.5 = 50/50)
P.cgh     = 0.247;      % CG height [m]
P.FLLTD   = 0.5;        % front lateral-load-transfer distribution (fraction on front)
P.cla     = 3.1;        % C_L * A  [m^2]  (downforce coeff x frontal area)
P.cp      = 0.4;        % aero balance: fraction of downforce on the FRONT axle

% ---- Mass / environment (SET THESE to your car) ----
P.m_total = 264;        % total mass incl. driver [kg]            
P.g       = 9.81;       % gravity [m/s^2]
P.rho     = 1.20;       % air density [kg/m^3]

% ---- Operating point used to set the MEAN corner loads ----
P.V_op    = 15;         % representative cornering speed [m/s]     
P.Ay      = 1.2;        % lateral accel at the operating point [g] 

% ---- Suspension / tyre vertical (per corner) ----
P.m_u     = 5.7;         % unsprung mass per corner [kg]            
P.k_t     = 110e3;      % tyre vertical stiffness [N/m] (~90-130 N/mm) <-- CONFIRM
P.zeta    = 0.7;       % body-mode damping ratio (held constant in sweep)

% ---- Spring (wheel-rate) sweep -- the ONLY swept lever ----
P.kw_front_base = 3.7843e4; % baseline FRONT wheel rate [N/m]
P.kw_rear_base  = 3.7843e4; % baseline REAR  wheel rate [N/m]
P.scale_sweep   = linspace(0.9, 6, 60);   % multiplies both axle wheel rates
P.scale_named   = [0.55 1.00 1.80];          % "soft / medium / stiff" reference pts
P.named_labels  = {'Soft','Medium','Stiff'};

% ---- Road spectrum (ISO 8608). Autocross asphalt ~ class A-B ----
P.Gd_n0 = 2e-6;         % roughness PSD at n0 [m^3]  (A~1e-6, B~4e-6, C~16e-6)
P.n0    = 0.1;          % reference spatial frequency [cyc/m]
P.w_iso = 2.0;          % PSD waviness exponent
P.n_lo  = 0.02;  P.n_hi = 12;  P.n_pts = 4000;   % spatial-freq integration grid

% ---- Lap-time mapping ----
P.w_lat   = 0.65;       % weight of lateral grip in the combined grip index
P.w_long  = 0.35;       % weight of longitudinal grip
P.lt_exp  = 0.50;       % t ~ G^(-lt_exp); 0.5 = cornering/grip-limited autocross
% =============================================================================

%% ===== 1) Tyre load-sensitivity curves: muX(Fz), muY(Fz) and Fmax(Fz) ========
Fz_grid = linspace(150, 1600, 40);      % Fz operating range [N]  (adjust to tyre)
k_sweep = linspace(0, 0.25, 60);        % slip-ratio sweep for longitudinal peak
a_sweep = deg2rad(linspace(0, 14, 60)); % slip-angle  sweep for lateral peak

FxMax = zeros(size(Fz_grid));           % peak |Fx| vs Fz [N]
FyMax = zeros(size(Fz_grid));           % peak |Fy| vs Fz [N]
for i = 1:numel(Fz_grid)
    fz = Fz_grid(i);
    fx = zeros(size(k_sweep));
    for j = 1:numel(k_sweep)
        fxj = brushTireForce(k_sweep(j), 0, -fz);   % pure longitudinal (a=0)
        fx(j) = abs(fxj);
    end
    FxMax(i) = max(fx);
    fy = zeros(size(a_sweep));
    for j = 1:numel(a_sweep)
        [~, fyj] = brushTireForce(0, a_sweep(j), -fz); % pure lateral (k=0)
        fy(j) = abs(fyj);
    end
    FyMax(i) = max(fy);
end
muX = FxMax ./ Fz_grid;                 % peak grip coefficient vs load
muY = FyMax ./ Fz_grid;

%% ===== 2) Corner MEAN loads at the operating point ===========================
[Fz_corner, info] = corner_loads(P);    % [FL FR RL RR]  (inner/outer L-corner)
corner_names = {'FL(in)','FR(out)','RL(in)','RR(out)'};

% Per-corner sprung mass for the quarter-car (from STATIC load, mass is fixed)
m_s_front = info.static_front/P.g - P.m_u;
m_s_rear  = info.static_rear /P.g - P.m_u;
m_s_corner = [m_s_front m_s_front m_s_rear m_s_rear];
kw_base    = [P.kw_front_base P.kw_front_base P.kw_rear_base P.kw_rear_base];

%% ===== 3) + 4) Sweep spring rate -> sigma_Fz -> effective mu =================
nS = numel(P.scale_sweep);
muX_eff = zeros(nS,4); muY_eff = zeros(nS,4);
sig_Fz  = zeros(nS,4); fride_f = zeros(nS,1);
for s = 1:nS
    sc = P.scale_sweep(s);
    for c = 1:4
        kw   = kw_base(c)*sc;
        sg   = sigma_Fz(kw, m_s_corner(c), P);           % RMS dynamic load [N]
        sig_Fz(s,c) = sg;
        muX_eff(s,c) = eff_mu(Fz_corner(c), sg, Fz_grid, FxMax);
        muY_eff(s,c) = eff_mu(Fz_corner(c), sg, Fz_grid, FyMax);
    end
    % front ride frequency for the x-axis (informational)
    krf = (kw_base(1)*sc)*P.k_t/((kw_base(1)*sc)+P.k_t);
    fride_f(s) = 1/(2*pi)*sqrt(krf/m_s_front);
end

% Car-level, load-weighted effective grip and combined grip index
wgt = Fz_corner/sum(Fz_corner);
muX_car = muX_eff*wgt(:);
muY_car = muY_eff*wgt(:);
G_index = P.w_lat*muY_car + P.w_long*muX_car;

% Reference to the STIFFEST setup (sigma_Fz largest there) so deltas are
% "what you gain by going softer". Quasi-static ceiling also reported.
[~,iref] = max(P.scale_sweep);  Gref = G_index(iref);
% dLap_pct = -P.lt_exp*(G_index/Gref - 1)*100;     % +ve = faster than stiff ref

% Quasi-static ceiling (sigma_Fz -> 0): the grip you'd have with zero CPL
muX0 = arrayfun(@(c) eff_mu(Fz_corner(c),0,Fz_grid,FxMax),1:4)*wgt(:);
muY0 = arrayfun(@(c) eff_mu(Fz_corner(c),0,Fz_grid,FyMax),1:4)*wgt(:);
G0   = P.w_lat*muY0 + P.w_long*muX0;


%% ===== Plots =================================================================
% figure('Color','w','Position',[80 80 1100 760]);
% 
% subplot(1,3,1);
% plot(Fz_grid,muX,'-o','LineWidth',1.4); hold on; plot(Fz_grid,muY,'-s','LineWidth',1.4);
% xline(mean(Fz_corner),'k--','mean Fz'); grid on;
% xlabel('F_z [N]'); ylabel('peak \mu'); legend('\mu_x','\mu_y','Location','best');
% title('1) Tyre load sensitivity (from model)');
% 
% subplot(1,3,2);
% plot(fride_f, mean(sig_Fz./Fz_corner,2)*100,'-','LineWidth',1.6); grid on;
% xlabel('front ride freq [Hz]'); ylabel('\sigma_{Fz}/F_z  [%]');
% title('2) CPL variation vs spring rate');
% 
% subplot(1,3,3);
% plot(fride_f,muX_car,'-','LineWidth',1.6); hold on;
% plot(fride_f,muY_car,'-','LineWidth',1.6); grid on;
% xlabel('front ride freq [Hz]'); ylabel('effective car \mu');
% legend('\mu_x eff','\mu_y eff','Location','best');
% title('3) Effective mechanical grip vs spring rate');

% subplot(2,2,4);
% plot(fride_f,dLap_pct,'-','LineWidth',1.8); grid on; hold on;
% [mx,imx]=max(dLap_pct); plot(fride_f(imx),mx,'ro','MarkerFaceColor','r');
% text(fride_f(imx),mx,sprintf('  opt %.2fHz',fride_f(imx)));
% xlabel('front ride freq [Hz]'); ylabel('\Delta lap time [%] (+ = faster)');
% title('4) Lap-time gain vs spring rate (rel. to stiffest)');

figure()
plot(fride_f, muY_car, LineWidth=2)
xlabel("Ride Freq/Hz")
ylabel("CPL-Averaged Effective \mu")
title("Effects of Ride Freq on Effective \mu")
ylim([1.545, 1.548])

%% ============================ FUNCTIONS ======================================
function [Fz, info] = corner_loads(P)
% MEAN per-corner vertical load at the cornering operating point.
% Order: [FL(inner) FR(outer) RL(inner) RR(outer)] for a left-hand corner.
    g = P.g;
    static_front = P.m_total*g*P.w_bias;        % front axle static [N]
    static_rear  = P.m_total*g*(1-P.w_bias);
    sf = static_front/2;  sr = static_rear/2;   % per-corner static

    DF = 0.5*P.rho*P.V_op^2*P.cla;              % total downforce [N]
    af = DF*P.cp/2;  ar = DF*(1-P.cp)/2;        % aero per corner

    dW_total = P.m_total*P.Ay*g*P.cgh/P.tw;     % total lateral load transfer [N]
    dWf = P.FLLTD*dW_total;                     % front-axle transfer
    dWr = (1-P.FLLTD)*dW_total;

    Fz = [ sf+af-dWf ,  sf+af+dWf ,  sr+ar-dWr ,  sr+ar+dWr ];
    Fz = max(Fz, 1);                            % guard against lift-off (clamp)
    info.static_front = static_front; info.static_rear = static_rear;
    info.DF = DF;
end

function sg = sigma_Fz(kw, m_s, P)
% RMS dynamic tyre-load variation from a quarter-car driven by an ISO-8608
% road PSD. kw = wheel (suspension) rate [N/m]; m_s = corner sprung mass [kg].
    k_t = P.k_t; m_u = P.m_u;
    k_ride = kw*k_t/(kw+k_t);
    c = 2*P.zeta*sqrt(k_ride*m_s);              % body-mode damping, constant zeta

    n  = linspace(P.n_lo, P.n_hi, P.n_pts);     % spatial frequency [cyc/m]
    Gd = P.Gd_n0*(n/P.n0).^(-P.w_iso);          % road displacement PSD [m^3]

    w  = 2*pi*P.V_op*n;                          % temporal angular freq [rad/s]
    s  = 1i*w;
    A  = kw + c*s;
    Den = (m_u*s.^2 + A + k_t).*(m_s*s.^2 + A) - A.^2;
    Zu_Zr = k_t.*(m_s*s.^2 + A)./Den;            % road->unsprung motion
    H_Fz  = k_t.*(Zu_Zr - 1);                    % road->dynamic tyre force [N/m]

    PSD_Fz = (abs(H_Fz).^2).*Gd;                 % output PSD over spatial freq
    var_Fz = trapz(n, PSD_Fz);                   % variance [N^2]
    sg = sqrt(max(var_Fz,0));
end

function mu_e = eff_mu(Fz_mean, sigma, Fz_grid, Fmax_grid)
% Effective peak grip = E[ Fmax(Fz) ] / Fz_mean, averaging the tyre's peak-force
% curve over a (truncated-normal) load distribution of mean Fz_mean, std sigma.
% Concavity of Fmax(Fz) (load sensitivity) makes variation reduce the average.
    if sigma < 1e-6
        mu_e = interp1(Fz_grid, Fmax_grid, Fz_mean,'linear','extrap')/Fz_mean;
        return;
    end
    lo = max(Fz_mean-5*sigma, 1);  hi = Fz_mean+5*sigma;
    z  = linspace(lo, hi, 401);
    pdf = exp(-0.5*((z-Fz_mean)/sigma).^2);
    pdf = pdf/trapz(z,pdf);                       % renormalise (truncation)
    Fm = interp1(Fz_grid, Fmax_grid, z,'linear','extrap');
    Fm = max(Fm,0);
    Fbar = trapz(z, Fm.*pdf);                      % expected peak force [N]
    mu_e = Fbar/Fz_mean;
end
