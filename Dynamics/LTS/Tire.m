classdef Tire
    properties
        fy_fixed_params
        fy_d_list
        fz_list
        fx_params
    end
    methods
        function obj = Tire(fy_fixed_params, fy_d_list, fz_list, fx_params)
            obj.fy_fixed_params = fy_fixed_params;
            obj.fy_d_list = fy_d_list;
            obj.fz_list = fz_list;
            obj.fx_params = fx_params;
        end

        function fy_pure = Fy_pure(obj, SA, FZ)
            if FZ > 0
                fy_pure = 0;
                return
            end
            MF_D = @(D, SA, FZ, fixedParams) ...
            D .* sin(fixedParams(1) .* atan(fixedParams(2) .* SA - ...
            fixedParams(3) .* (fixedParams(2) .* SA - atan(fixedParams(2) .* SA)))) .* FZ;
            D_current = interp1(obj.fz_list, obj.fy_d_list, FZ, 'spline', NaN);
            fy_pure = MF_D(D_current, SA, FZ, obj.fy_fixed_params);
        end

        function muy = muy(obj, FZ)
            D_current = interp1(obj.fz_list, obj.fy_d_list, clip(FZ, obj.fz_list(end), obj.fz_list(1)), 'spline', NaN);
            muy = D_current;
        end

        function mux = mux(obj)
            mux = -obj.fx_params(1);
        end

        function fx_pure = Fx_pure(obj, SR, FZ)
            params = obj.fx_params;
            fx_pure =  params(1) .* sin(params(2) .* atan(params(3) .* SR - params(4) .* (params(3) .* SR - atan(params(3) .* SR)))) .* FZ;
        end

        function r_loaded = RL(obj, FZ)
            r_loaded = (FZ * 0.0015 + 20.6101)/100; % conversion from cm to m
        end

        function output = output_combined(obj, SA, SR, FZ, Mt)
            fx_p = obj.Fx_pure(SR, FZ);
            fy_p = obj.Fy_pure(SA, FZ);
            r_sq = (fx_p/(2.54 * FZ))^2 + (fy_p/(2.7 * FZ))^2; % approximate max mu_x and mu_y for a very crude friction ellipse
            if r_sq <= 1 % see if combined pure forces exceed friction limit
                f_combined = 2/3 * [fx_p, fy_p]; % rule of thumb from TTC forum that you scale TTC data by 2/3
            else
                f_combined = 2/3 * sqrt(1/r_sq) * [fx_p, fy_p];
            end
            r_loaded = obj.RL(FZ);
            moi = 0.00005670 + 0.00124908 + 0.08236194; % moments of intertia of half shaft, hub, and rim+tire in the rotational axis
            wdot = (Mt - f_combined(1) * r_loaded)/moi;
            output = [f_combined, wdot];
        end
    end
end
          