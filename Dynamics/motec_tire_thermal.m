LC0_endur = load('LC0_motec_endur_2.mat');
T_ambient = 20;
% LC0_autox = load('LC0_motec_autox_thermal.mat');
% R20_endur = load('R20_motec_endur_thermal.mat');
% R20_autox = load('R20_motec_autox_thermal.mat');

TT_LE = tAvg(LC0_endur);
time = LC0_endur.TT_FL_1.Time;
time = time - time(1); % start at 0

model = @(P, t) Temp(t, P(1), P(2), P(3), T_ambient, P(4));
P0 = [1, 1, 10 28];
TT_LE_smooth = smoothdata(TT_LE, 'movmean', 20);

figure()
plot(TT_LE_smooth)
hold on
plot(TT_LE)
hold off

p_fit = lsqcurvefit(model, P0, time, TT_LE_smooth)

figure()
plot(time, model(p_fit, time))
hold on
plot(time, TT_LE)
hold off

function temp = Temp(t, P_in, gamma, k, Ta, T0)
    temp = Ta + (P_in - (P_in + gamma * (Ta - T0)) * exp(-gamma/k * t))/gamma;
end

function TT_avg = tAvg(motecRun)
    fields = fieldnames(motecRun);

    flFields = fields(startsWith(fields, 'TT_FL_'));
    frFields = fields(startsWith(fields, 'TT_FR_'));
    rlFields = fields(startsWith(fields, 'TT_RL_'));
    rrFields = fields(startsWith(fields, 'TT_RR_'));

    fl_sum = zeros(1, length(motecRun.TT_FL_1));
    fr_sum = zeros(1, length(motecRun.TT_FL_1));
    rl_sum = zeros(1, length(motecRun.TT_FL_1));
    rr_sum = zeros(1, length(motecRun.TT_FL_1));

    for i=1:16
        fl_sum = fl_sum + motecRun.(flFields{i}).Value;
        fr_sum = fr_sum + motecRun.(frFields{i}).Value;
        rl_sum = rl_sum + motecRun.(rlFields{i}).Value;
        rr_sum = rr_sum + motecRun.(rrFields{i}).Value;
    end

    fl_avg = fl_sum./16;
    fr_avg = fr_sum./16;
    rl_avg = rl_sum./16;
    rr_avg = rr_sum./16;

    TT_avg = (fl_avg + fr_avg + rl_avg + rr_avg)/4;
end
