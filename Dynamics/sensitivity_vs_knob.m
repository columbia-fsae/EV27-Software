% Knobs: brake bias, toe angle, FLLTD, cp (abstract decoupled)
% Different bars: controllability @ entry & exit (non zero ax), stability @ limit,
init_globals
tw = 1.2;
w_bias = 0.5;
cgh = 0.247;
FLLTD = 0.5;
cla = 3.1;
cp = 0.4;
toe_f = 0;
toe_r = 0;
v = 10;
ax_target = 0;
b_bias = 0.5;

%% b bias
ymd_ce = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_ce_2 = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias-0.2);
[N_stb_ce_bb, N_ctr_ce_bb] = ymd_ce.cs_derivatives(0, 0); % corner entry
[N_stb_ce_bb_2, N_ctr_ce_bb_2] = ymd_ce_2.cs_derivatives(0, 0); % corner entry
clear ymd_ce ymd_ce_2

%% front toe
% corner entry
ymd_ce = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_ce_2 = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f - 0.2, toe_r, b_bias); % toe out
[N_stb_ce_ft, N_ctr_ce_ft] = ymd_ce.cs_derivatives(0, 0);
[N_stb_ce_ft_2, N_ctr_ce_ft_2] = ymd_ce_2.cs_derivatives(0, 0);
% limit
ymd_lim = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_lim_2 = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f - 0.2, toe_r, b_bias); % toe out
[N_stb_lim_ft, N_ctr_lim_ft] = ymd_lim.cs_derivatives(-5, 12);
[N_stb_lim_ft_2, N_ctr_lim_ft_2] = ymd_lim_2.cs_derivatives(-5, 12);
clear ymd_ce ymd_ce_2 ymd_lim ymd_lim_2

%% rear toe
% corner exit (+ 0.5 g)
ymd_cx = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_cx_2 = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD, cla, cp, toe_f, toe_r + 0.2, b_bias); % toe out
[N_stb_cx_rt, N_ctr_cx_rt] = ymd_cx.cs_derivatives(0, 0);
[N_stb_cx_rt_2, N_ctr_cx_rt_2] = ymd_cx_2.cs_derivatives(0, 0);
% limit
ymd_lim = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_lim_2 = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r + 0.2, b_bias); % toe out
[N_stb_lim_rt, N_ctr_lim_rt] = ymd_lim.cs_derivatives(-5, 12);
[N_stb_lim_rt_2, N_ctr_lim_rt_2] = ymd_lim_2.cs_derivatives(-5, 12);
clear ymd_cx ymd_cx_2 ymd_lim ymd_lim_2

%% FLLTD
% corner entry
ymd_ce = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_ce_2 = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD - 0.06, cla, cp, toe_f, toe_r, b_bias);
[N_stb_ce_ltd, N_ctr_ce_ltd] = ymd_ce.cs_derivatives(0, 0);
[N_stb_ce_ltd_2, N_ctr_ce_ltd_2] = ymd_ce_2.cs_derivatives(0, 0);
% corner exit (+ 0.5 g)
ymd_cx = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_cx_2 = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD - 0.06, cla, cp, toe_f, toe_r, b_bias);
[N_stb_cx_ltd, N_ctr_cx_ltd] = ymd_cx.cs_derivatives(0, 0);
[N_stb_cx_ltd_2, N_ctr_cx_ltd_2] = ymd_cx_2.cs_derivatives(0, 0);
% limit
ymd_lim = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_lim_2 = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD - 0.06, cla, cp, toe_f, toe_r, b_bias);
[N_stb_lim_ltd, N_ctr_lim_ltd] = ymd_lim.cs_derivatives(-5, 12);
[N_stb_lim_ltd_2, N_ctr_lim_ltd_2] = ymd_lim_2.cs_derivatives(-5, 12);
clear ymd_ce ymd_ce_2 ymd_cx ymd_cx_2 ymd_lim ymd_lim_2

%% CP
% corner entry
ymd_ce = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_ce_2 = YMDSolver(tw, w_bias, cgh, v, -0.5, FLLTD, cla, cp - 0.2, toe_f, toe_r, b_bias);
[N_stb_ce_cp, N_ctr_ce_cp] = ymd_ce.cs_derivatives(0, 0);
[N_stb_ce_cp_2, N_ctr_ce_cp_2] = ymd_ce_2.cs_derivatives(0, 0);
% corner exit (+ 0.5 g)
ymd_cx = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_cx_2 = YMDSolver(tw, w_bias, cgh, v, 0.5, FLLTD, cla, cp - 0.2, toe_f, toe_r, b_bias);
[N_stb_cx_cp, N_ctr_cx_cp] = ymd_cx.cs_derivatives(0, 0);
[N_stb_cx_cp_2, N_ctr_cx_cp_2] = ymd_cx_2.cs_derivatives(0, 0);
% limit
ymd_lim = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_lim_2 = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp - 0.2, toe_f, toe_r, b_bias);
[N_stb_lim_cp, N_ctr_lim_cp] = ymd_lim.cs_derivatives(-5, 12);
[N_stb_lim_cp_2, N_ctr_lim_cp_2] = ymd_lim_2.cs_derivatives(-5, 12);
clear ymd_ce ymd_ce_2 ymd_cx ymd_cx_2 ymd_lim ymd_lim_2

%% Plotting: sensitivity bar charts by cornering scenario
% For each knob: delta = (perturbed - baseline), signed, separately for
% stability (N_beta) and control (N_delta). Bar direction shows whether the
% perturbation pushed the derivative up or down.

set(groot, 'defaultAxesFontSize', 16)


% ---- Corner Entry ----
ce_knobs = {'Brake Bias','Front Toe','FLLTD','Aero Balance'};
ce_stb = abs([N_stb_ce_bb_2  - N_stb_ce_bb, ...
          N_stb_ce_ft_2  - N_stb_ce_ft, ...
          N_stb_ce_ltd_2 - N_stb_ce_ltd, ...
          N_stb_ce_cp_2  - N_stb_ce_cp]);
ce_ctr = abs([N_ctr_ce_bb_2  - N_ctr_ce_bb, ...
          N_ctr_ce_ft_2  - N_ctr_ce_ft, ...
          N_ctr_ce_ltd_2 - N_ctr_ce_ltd, ...
          N_ctr_ce_cp_2  - N_ctr_ce_cp]);

figure('Name','Corner Entry Sensitivity');
X = reordercats(categorical(ce_knobs), ce_knobs);
bar(X, [ce_stb; ce_ctr]');
ylabel('Absolute Change in Derivative[Nm/deg]');
title('Corner Entry (-0.5 g)','FontSize',16);
legend('Stability  \DeltaN_\beta','Control  \DeltaN_\delta');
yline(0,'k-'); grid on

% ---- Corner Exit (+0.5 g) ----
cx_knobs = {'Rear Toe','FLLTD','Aero Balance'};
cx_stb = abs([N_stb_cx_rt_2  - N_stb_cx_rt, ...
          N_stb_cx_ltd_2 - N_stb_cx_ltd, ...
          N_stb_cx_cp_2  - N_stb_cx_cp]);
cx_ctr = abs([N_ctr_cx_rt_2  - N_ctr_cx_rt, ...
          N_ctr_cx_ltd_2 - N_ctr_cx_ltd, ...
          N_ctr_cx_cp_2  - N_ctr_cx_cp]);

figure('Name','Corner Exit Sensitivity');
X = reordercats(categorical(cx_knobs), cx_knobs);
bar(X, [cx_stb; cx_ctr]');
ylabel('Absolute Change in Derivative[Nm/deg]');
title('Corner Exit (+0.5 g)','FontSize',16);
legend('Stability  \DeltaN_\beta','Control  \DeltaN_\delta');
yline(0,'k-'); grid on

% ---- Limit ----
lim_knobs = {'Front Toe','FLLTD','Rear Toe','Aero Balance'};
lim_stb = abs([N_stb_lim_ft_2  - N_stb_lim_ft, ...
           N_stb_lim_ltd_2 - N_stb_lim_ltd, ...
           N_stb_lim_rt_2  - N_stb_lim_rt, ...
           N_stb_lim_cp_2  - N_stb_lim_cp]);
lim_ctr = abs([N_ctr_lim_ft_2  - N_ctr_lim_ft, ...
           N_ctr_lim_ltd_2 - N_ctr_lim_ltd, ...
           N_ctr_lim_rt_2  - N_ctr_lim_rt, ...
           N_ctr_lim_cp_2  - N_ctr_lim_cp]);

figure('Name','Limit Sensitivity');
X = reordercats(categorical(lim_knobs), lim_knobs);
bar(X, [lim_stb; lim_ctr]');
ylabel('Absolute Change in Derivative[Nm/deg]');
title('Limit (@ Ay = 1.5 g)','FontSize',16);
legend('Stability  \DeltaN_\beta','Control  \DeltaN_\delta');
yline(0,'k-'); grid on

%%

figure()
ax = gca;

ymd = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_2 = YMDSolver(tw, 0.53, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
ymd_3 = YMDSolver(tw, w_bias, cgh * 1.2, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);

ymd.ymd_plot('green')
lines_after_first = findobj(ax, 'Type', 'line');
h1 = lines_after_first(1);   % one green line

ymd_2.ymd_plot('red')
lines_after_second = findobj(ax, 'Type', 'line');
h2 = lines_after_second(1);  % one red line (newest is first)

ymd_3.ymd_plot('blue')
lines_after_third = findobj(ax, 'Type', 'line');
h3 = lines_after_third(1);  % one red line (newest is first)

legend([h1 h2 h3], "Baseline (50-50 Weight Bias)", "53% Front Weight Bias", "1.2x Baseline CGH")
