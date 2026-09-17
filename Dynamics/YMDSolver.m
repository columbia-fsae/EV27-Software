classdef YMDSolver
    % YMDSolver - Yaw Moment Diagram solver for vehicle dynamics analysis
    properties
        % Vehicle geometry
        wb      = 1.530     % wheelbase (m)
        tw                  % track width (m)
        a                   % CG to front axle (m)
        b                   % CG to rear axle (m)

        % Mass / weight
        m       = 264       % mass (kg)
        g       = 9.81      % gravity (m/s^2)
        w                   % total weight (N)
        w_static_f          % static weight per front tire (N)
        w_static_r          % static weight per rear tire (N)

        % CG
        cgh                 % CG height (m)

        % Aero
        cla                 % total aero lift coefficient
        cp                  % aero center of pressure (fraction front)
        F_df_f              % front aero downforce (N)
        F_df_r              % rear aero downforce (N)

        % Suspension / setup
        FLLTD               % front lateral load transfer distribution (fraction)
        toe_f               % front toe angle
        toe_r               % rear toe angle
        b_bias              % brake bias

        % Operating point
        v                   % velocity (m/s)
        ax_target
    end

    methods
        %% Constructor
        function obj = YMDSolver(tw, w_bias, cgh, v, ax_target, FLLTD, cla, cp, toe_f, toe_r, b_bias)
            obj.tw     = tw;
            obj.cgh    = cgh;
            obj.v      = v;
            obj.ax_target = ax_target;
            obj.FLLTD  = FLLTD;
            obj.cla    = cla;
            obj.cp     = cp;
            obj.toe_f  = toe_f;
            obj.toe_r  = toe_r;
            obj.b_bias = b_bias;

            % Geometry derived from w_bias
            obj.a = obj.wb * (1 - w_bias);   % CG to front axle
            obj.b = obj.wb * w_bias;          % CG to rear axle

            % Weight
            obj.w          = obj.m * obj.g;
            obj.w_static_f = obj.w * w_bias       / 2;
            obj.w_static_r = obj.w * (1 - w_bias) / 2;

            % Aero downforce split (pre-computed; fixed for this velocity)
            rho         = 1.225;  % air density (kg/m^3)
            obj.F_df_f  = obj.cla * obj.v^2 * obj.cp * rho / 2;
            obj.F_df_r  = obj.cla * obj.v^2 * (1 - obj.cp) * rho / 2;
        end


        %% YMD plotting
        function ymd_plot(obj, color)
            % Sweep parameters
            delta_isolist = linspace(-10, 10, 20); % steering angle (degrees) # changed to get rid of weird lines?
            beta_isolist = linspace(-5, 5, 20);     % sideslip angle (degrees)
            
            % % Check if we need combined slip (ax_target != 0)
            % % NOTE: Combined slip requires Fx() and Fy_combined() methods in tire model
            % use_combined_slip = (abs(ax_target) > 1e-6);
            % 
            % if use_combined_slip
            %     warning('Combined slip requested but tire model lacks Fx() and Fy_combined() methods. Using pure lateral only.');
            %     use_combined_slip = false;
            % end
            
            % Plot Beta Isolines (constant beta, varying delta)
            for beta = beta_isolist
                ay_list = zeros(1, length(delta_isolist));
                Mz_list = zeros(1, length(delta_isolist));
                ay_idx = 1;
                
                for delta = delta_isolist
                    [ay_converged, Mz_converged] = obj.solve_combined_slip(beta, delta);
                    fprintf("delta = %.2f ", delta)
                    ay_list(ay_idx) = ay_converged;
                    Mz_list(ay_idx) = Mz_converged;
                    ay_idx = ay_idx + 1;
                end
                
                plot(ay_list, Mz_list, 'Color', color, 'LineWidth', 1.5)
                hold on
        
                % text(ay_list(end), Mz_list(end), ...
                %  sprintf('\\beta = %.1f°', beta), ...
                %  'Color', color, ...
                %  'FontSize', 8)
            end
            
            % Plot Delta Isolines (constant delta, varying beta)
            for delta = delta_isolist
                ay_list = zeros(1, length(beta_isolist));
                Mz_list = zeros(1, length(beta_isolist));
                ay_idx = 1;
                
                for beta = beta_isolist
                    [ay_converged, Mz_converged] = obj.solve_combined_slip(beta, delta);
                    fprintf("beta = %.2f ", beta)            
                    ay_list(ay_idx) = ay_converged;
                    Mz_list(ay_idx) = Mz_converged;
                    ay_idx = ay_idx + 1;
                end
                
                plot(ay_list, Mz_list, 'Color', color, 'LineWidth', 1.5)
                hold on
        
                % text(ay_list(end), Mz_list(end), ...
                %  sprintf('\\delta = %.1f°', delta), ...
                %  'Color', color, ...
                %  'FontSize', 8)
            end

            xlabel("A_y/g", "FontSize",14)
            ylabel("Mz/Nm", "FontSize",14)
        end
        
        %% control & stability derivatives
        function [N_beta, N_delta] = cs_derivatives(obj, beta, delta)
            d_beta = 0.01;
            [~, mz_beta_i] = obj.solve_combined_slip(beta - d_beta/2, delta);
            [~, mz_beta_f] = obj.solve_combined_slip(beta + d_beta/2, delta);

            d_delta = 0.01;
            [~, mz_delta_i] = obj.solve_combined_slip(beta, delta - d_delta/2);
            [~, mz_delta_f] = obj.solve_combined_slip(beta, delta + d_delta/2);

            N_beta = (mz_beta_f - mz_beta_i)/d_beta;
            N_delta = (mz_delta_f - mz_delta_i)/d_delta;
        end
        
        %% Combined slip solver
        function [ay_converged, Mz_converged, FY_vehicle_frame, FZ] = solve_combined_slip(obj, beta, delta)
            
            ay_current = 1;  % initial guess
            diff_ay = 1;
            
            % Slip ratio search range
            if obj.ax_target < 0
                sr_range = linspace(-0.11, 0, 100);  % braking
            else
                sr_range = linspace(0, 0.11, 100);   % accelerating
            end
        
            iter = 0;
            max_iter = 400;

            chi = 1/obj.b_bias - 1; % defined so that f_r = chi * f_f for braking torque, so don't do 0% brake bias 

            pure_lateral = abs(obj.ax_target) < 1e-6; % no longitudinal target -> SR stays zero, skip the sr scan
            
            while diff_ay > 1e-3 && iter < max_iter
                % Longitudinal load transfer
                longLT = obj.w * obj.ax_target * obj.cgh / obj.wb;
                
                % Lateral load transfer
                LLT = obj.w * ay_current * obj.cgh / obj.tw;
                F_LLT = obj.FLLTD * LLT;
                R_LLT = LLT - F_LLT;
                
                % Vertical loads (static + lateral + longitudinal + aero)
                Fz_fl = obj.w_static_f + F_LLT - longLT / 2 + obj.F_df_f / 2;
                Fz_fr = obj.w_static_f - F_LLT - longLT / 2 + obj.F_df_f / 2;
                Fz_rl = obj.w_static_r + R_LLT + longLT / 2 + obj.F_df_r / 2;
                Fz_rr = obj.w_static_r - R_LLT + longLT / 2 + obj.F_df_r / 2;
                FZ = [Fz_fl, Fz_fr, Fz_rl, Fz_rr];
                FZ = max(FZ, 0); % clamp: a lifted/unloaded tire makes ~0 force, not sign-flipped force
                
                Delta = [delta - obj.toe_f, delta + obj.toe_f, -obj.toe_r, obj.toe_r];
                r_target = ay_current * obj.g / (obj.v * cos(deg2rad(beta))); % ay_current is in g's -> convert to m/s^2 for physical yaw rate
                Alpha = YMDSolver.slip_ang(beta, obj.v, r_target, Delta(1), Delta(2), Delta(3), Delta(4), obj.a, obj.b, obj.tw);
                
                best_sr = 0;
                min_ax_error = inf;

                if pure_lateral
                    min_ax_error = 0; % SR = [0 0 0 0], longitudinal target trivially met
                else
                    for sr_test = sr_range
                        if sr_test < 0
                            % in the case of braking, sr_test is sr_front
                            sr_rear_test = chi * sr_test; 
                            SR = [sr_test, sr_test, sr_rear_test, sr_rear_test]; 
                        else 
                            SR = [0, 0, sr_test, sr_test];
                        end

                        % Calculate forces with combined slip
                        FX_vehicle_frame = zeros(1, 4);
                        FY_vehicle_frame = zeros(1, 4);

                        for tire_idx = 1:4
                            [fx, fy, ~, ~, ~, ~] = brushTireForce(SR(tire_idx), deg2rad(Alpha(tire_idx)), -FZ(tire_idx));
                            FY_vehicle_frame(tire_idx) = fx * sin(deg2rad(Delta(tire_idx))) + (-fy) * cos(deg2rad(Delta(tire_idx))); % sign flip cuz newton's 3rd
                            FX_vehicle_frame(tire_idx) = fx * cos(deg2rad(Delta(tire_idx))) - (-fy) * sin(deg2rad(Delta(tire_idx)));
                        end

                        ax_achieved = sum(FX_vehicle_frame) / obj.w;
                        ax_error = abs(ax_achieved - obj.ax_target);

                        if ax_error < min_ax_error % Chris: linear scan in sr_range that completely solves for sr
                            min_ax_error = ax_error;
                            best_sr = sr_test;
                        end
                    end
                end
                % NOTE: feasibility (min_ax_error) is now checked AFTER ay converges,
                % so a transient mismatch on an early iteration no longer kills the solve.
                if best_sr < 0
                    best_sr_rear = chi * best_sr;
                    SR = [best_sr, best_sr, best_sr_rear, best_sr_rear]; % Chris: separate cases for braking & accel
                else
                    SR = [0, 0, best_sr, best_sr];
                end
        
                FX_vehicle_frame = zeros(1, 4);
                FY_vehicle_frame = zeros(1, 4);
                
                for tire_idx = 1:4
                    [fx, fy, ~, ~, ~, ~] = brushTireForce(SR(tire_idx), deg2rad(Alpha(tire_idx)), -FZ(tire_idx));
                    % fprintf("slip angle is %.2f deg \n", Alpha(tire_idx));
                    % fprintf("fy at tire is %.2f N \n", fy);
                    FY_vehicle_frame(tire_idx) = fx * sin(deg2rad(Delta(tire_idx))) + (-fy) * cos(deg2rad(Delta(tire_idx))); % newton's 3rd
                    % fprintf("tire %d produce fy_veh of %.2f N with slip angle %.2f \n", tire_idx, FY_vehicle_frame(tire_idx), Alpha(tire_idx));
                    FX_vehicle_frame(tire_idx) = fx * cos(deg2rad(Delta(tire_idx))) - (-fy) * sin(deg2rad(Delta(tire_idx))); 
                end
                
                ay_new = sum(FY_vehicle_frame) / obj.w;
                % fprintf("fy_total is %.2f \n", sum(FY_vehicle_frame));
                diff_ay = abs(ay_new - ay_current);
                lambda = 0.8;  % relaxation factor (0 < lambda ≤ 1); numerical dampening from chat
                ay_current = lambda * ay_new + (1 - lambda) * ay_current;
                iter = iter + 1;
            end
            
            % disp("Fz and Fy");
            % disp(FZ);
            % disp(FY_vehicle_frame);

        
            if iter >= max_iter
                warning('ay solver did not converge');
            % else
            %     disp("ay converged")
            end

            % Feasibility check AFTER ay has converged (moved out of the loop).
            % Uses the last iteration's best match, which reflects the converged ay.
            if min_ax_error > 0.05
                warning("Ax target not achieved")
                ay_converged = nan;
                Mz_converged = nan;
                return
            end
            
            Mz = (FY_vehicle_frame(1) + FY_vehicle_frame(2)) * obj.a - ...
                 (FY_vehicle_frame(3) + FY_vehicle_frame(4)) * obj.b +  ...
                 (FX_vehicle_frame(1) + FX_vehicle_frame(3) - FX_vehicle_frame(2) - FX_vehicle_frame(4)) * obj.tw/2;
            
            ay_converged = ay_current;
            Mz_converged = Mz;
        end
    end

    methods (Static)
        %% Slip Angle Calculation
        function alpha = slip_ang(beta, v, r, delta_fl, delta_fr, delta_rl, delta_rr, a, b, tw)
            beta_rad = deg2rad(beta);
            ydot = v * sin(beta_rad);
            xdot = v * cos(beta_rad);
            
            alpha_fl = atan((ydot + a * r) / (xdot + r * tw / 2)) - deg2rad(delta_fl);
            alpha_fr = atan((ydot + a * r) / (xdot - r * tw / 2)) - deg2rad(delta_fr);
            alpha_rl = atan((ydot - b * r) / (xdot + r * tw / 2)) - deg2rad(delta_rl);
            alpha_rr = atan((ydot - b * r) / (xdot - r * tw / 2)) - deg2rad(delta_rr);
            
            alpha = rad2deg([alpha_fl, alpha_fr, alpha_rl, alpha_rr]);
        end 
    end
end