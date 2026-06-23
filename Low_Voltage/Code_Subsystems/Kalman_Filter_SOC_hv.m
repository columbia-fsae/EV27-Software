classdef Kalman_Filter_SOC_hv < matlab.System
    
    % Public, tunable properties
    properties
       qgain_1=1; 
       qgain_2=1;
       qgain_soc=1;
       qgain = 1;
       N = 100;
    end
    properties(Nontunable)
        r0 = 0.03;
    end
    % Pre-computed constants or internal states
    properties (Access = private)
        %State Space Matrices
        a
        b
        c
        
        %Process Covariance Update
        d
        d_window
        D

        %Output equation
        g
        
        %Parameter Matrices
        c_theta

        %Kalman Gain
        k
        k_theta
       
        %Battery Values
        
        r_ct
        r_dif
        c_ct
        c_dif
        
        %Battery Table Functions
        ke
        ocv

        q_nom = 1*3.3*3600; %8*2.6; %Capacity of eight parallel cells, 2600mAh
        
        % Error covariance matrices
        q_fixed   % Process noise
        q_theta_fixed
        r %sensor noise
        r_theta
        
        % Initial conditions
        dx_dtheta
        I_prev
        q_prev
        x
        theta
        p
        p_theta
        
        
        %Kalman Equations
        predX
        predP
        predTheta
        predPTheta
        updateX
        updateK
        updateP

        %Time and Counter
        t
        j
       
        z
        ocv_inv
    end

    methods (Access = protected)
        function setupImpl(obj)
            % Initialize Vectors
            obj.x = [0.3;0;0]; % Initial state, SoC,Vct,Vdif
            obj.theta = [obj.r0;obj.q_nom]; % Parameters Vector R_0 Q_nom
            obj.p = [1e-3 0 0;0 1e-3 0;0 0 1e-3]; % Initial error covariance
            obj.p_theta = [0.1 0; 0 0.001];
            
            obj.k = zeros(3,2);
            obj.k_theta = [1 1;1 1];
            obj.c_theta = [0 0;0 0];
            obj.j=1;

            %Process Covariance Update
            obj.d_window = zeros(1,obj.N);
            obj.I_prev = 0;
            obj.q_prev = obj.q_nom;
            obj.dx_dtheta = zeros(3,2);

            obj.g = @(x, ocv, I,r0) [ocv - x(2) - x(3) + r0*I;x(1)];

            %Update Equations
            obj.updateK = @(p,c,r) (p*c')/(c*p*c' + r);      % K[k]   = PC^T/(CPC^T + R)
            obj.updateX = @(x,k,z,g) x + k*(z-g);        % MAYBE x[n|n] = x[n|n-1] + K[n](z[n] - Cx[n|n-1]-Du[n])
            obj.updateP = @(k,c,p) (eye(size(p)) - k*c)*p; % P[n|n] = (I-K[n]C)P[n|n-1]
            
            %Sensor Error Matrices Initialization
            obj.r = [1e-4 0;0 1e-3]; %from paper, think about, probably small
            obj.r_theta = [1 0;0 100]; %from paper
            obj.q_fixed = [obj.qgain_soc*1000*obj.r(1,1) 0 0;0 obj.qgain_1*0.1*obj.r(1,1) 0;...
                0 0 obj.qgain_2*0.01*obj.r(1,1)];
            
            %Second Order Battery Model Numbers
            
            obj.r_ct = @(T)0.003;
            obj.c_ct = @(T)1000; 
            obj.r_dif = @(T)0.006;
            obj.c_dif = @(T) 100000;

            %Battery Table SoC, OCV Functions
            bat_lookup = load('/Users/benjaminlevin/MATLAB/Projects/FormulaSAE/Kalman Filters/battery_lookup_lv_unique2.mat'); % Column 1 is SOC, 2 is OCV, 3 is dVdS (gradient)
            soc_lut = bat_lookup.battery_lookup(:,1);
            ocv_lut = bat_lookup.battery_lookup(:,2);
            slope_lut = bat_lookup.battery_lookup(:,3);
            obj.ke = @(soc,extrap) interp1(soc_lut, slope_lut, soc, 'linear',extrap); %place/slope? on curve of OCV/SOC interpolate the slope
            obj.ocv = @(soc,extrap) interp1(soc_lut,ocv_lut, soc, 'linear',extrap);
            obj.ocv_inv = @(ocv,extrap) interp1(ocv_lut,soc_lut, ocv, 'linear',extrap);
            
            % Time/Prediction updates;
            obj.predX = @(a,x,b,u) a*x + b*u;    % x[n+1|n] = Ax[n] + Bu[n]
            obj.predP = @(a,p,q) a*p*a' + q;        % P[n|n]   = AP[n|n]A^T + BQB^T
            obj.predTheta = @(theta) theta;
            obj.predPTheta = @(p,q) p + q;
            
            obj.t = 0;
            obj.z = 0.3;
            
        end

        function [v1,v2,SoC, e_ekf, v, V_ocv,theta1,theta2] = stepImpl(obj, V, i,T,t_new)

            % Implement algorithm. Calculate y as a function of input u and
            % internal states.
            obj.a = [1 0 0;0 exp(-(t_new-obj.t)/(obj.r_ct(T)*obj.c_ct(T))) 0;...
                0 0 exp(-(t_new-obj.t)/(obj.r_dif(T)*obj.c_dif(T)))];
           
            
            obj.q_theta_fixed = obj.qgain*[1e-8 0; 0 1e-12];
            q = obj.q_fixed;
            

            %Predict
            obj.theta = obj.predTheta(obj.theta);
            obj.p_theta = obj.predPTheta(obj.p_theta, obj.qgain*obj.q_theta_fixed);
            
            obj.b = [(t_new-obj.t)/obj.theta(2); -1*obj.r_ct(T)*(1-exp(-(t_new-obj.t)/(obj.r_ct(T)*obj.c_ct(T))));...
            -1*obj.r_dif(T)*(1-exp(-(t_new-obj.t)/(obj.r_dif(T)*obj.c_dif(T))))];

            obj.x = obj.predX(obj.a,obj.x,obj.b,obj.I_prev);
            obj.p = obj.predP(obj.a,obj.p,q);

            
            if obj.x(1) < 0.05
                extrap_ocv = 2.5;
                extrap_ke = 137.4725;
            elseif obj.x(1) > 0.95
                extrap_ocv = 4.2;
                extrap_ke = 4;
            else
                extrap_ocv = 4.2;
                extrap_ke = 4;   
            end
            
            if obj.ocv(obj.x(1),extrap_ocv) < 2.5
                extrap_soc = 0;
            elseif obj.ocv(obj.x(1),extrap_ocv) > 4.1769
                extrap_soc = 1;
            else
                extrap_soc = 1;
            end
            
            obj.x(1) = max(0, min(obj.x(1),1));

            obj.c = [obj.ke(obj.x(1),extrap_ke) -1 -1;1 0 0];
            obj.z = obj.ocv_inv(V - obj.theta(1)*i,extrap_soc);

            v = obj.g(obj.x, obj.ocv(obj.x(1),extrap_ocv), i, obj.theta(1));

            obj.dx_dtheta = [0 (-(t_new-obj.t)*obj.I_prev)/(obj.theta(2))^2;0 0;0 0] + obj.a*(obj.dx_dtheta-obj.k*obj.c_theta);
            obj.c_theta = [i 0;0 0] + obj.c*([0 (-(t_new-obj.t)*obj.I_prev)/(obj.theta(2))^2;0 0;0 0] + obj.dx_dtheta);
            
            obj.q_prev = obj.theta(2);
            obj.d = V - v(1);
            e_ekf = obj.d;
            obj.I_prev = i;
            obj.t = t_new;

            %Update
            
            obj.k = obj.updateK(obj.p, obj.c, obj.r);
            obj.x = obj.updateX(obj.x, obj.k, [V;obj.z], v);
            obj.p = obj.updateP(obj.k, obj.c, obj.p);
             
            %Process Covariance Update
            obj.d_window = [obj.d_window(2:end) obj.d];
            obj.D = (1/obj.N)*sum(obj.d_window.*obj.d_window);
            obj.q_fixed = obj.k*obj.D*obj.k';
           
            
            obj.k_theta = obj.updateK(obj.p_theta, obj.c_theta, obj.r_theta);
            obj.theta = obj.updateX(obj.theta, obj.k_theta, [V;obj.z], v);
            obj.p_theta = obj.updateP(obj.k_theta, obj.c_theta, obj.p_theta);
          

            obj.x(1) = max(0, obj.x(1));
            obj.theta(1) = max(0, obj.theta(1));

            
            theta1 = obj.theta(1);
            theta2 = obj.theta(2);
            obj.j=obj.j+1;
            v1= obj.x(2);
            v2 = obj.x(3);
            SoC = obj.x(1);
            V_ocv = obj.ocv(SoC,extrap_ocv);
            v = obj.g(obj.x,obj.ocv(obj.x(1),extrap_ocv),obj.I_prev,obj.theta(1));
          
        end

        function resetImpl(~)
            % Initialize / reset internal properties
            %obj.x = [0;0]; % Initial state, v,a
            %obj.p = [0 0;0 0]; % Initial error covariance
            %obj.i=1;
        end
    end
end
