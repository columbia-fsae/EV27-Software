classdef Kalman_Filter_SOC_lv < matlab.System
    
    % Public, tunable properties
    properties
       qgain_1=1; 
       qgain_2=1;
       qgain_soc=1;
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
        
        %Output Equation
        g

        %Kalman Gain
        k
        
        %Second Order Battery Model Numbers
        %lookup tables for all these constants based on temperature maybe they exist already
        r_ct = 0.003;
        c_ct = 1000; 
        r_dif = 0.006;
        c_dif = 100000;
        %Battery Table Functions
        ke
        ocv
        
        q_nom = 1*3.3*3600; %Capacity of one parallel cell, 3300mAh, *3600 to Coulombs

        % Error covariance matrices
        q_fixed   % Process noise
        r %sensor noise
        
        % Initial conditions
        x
        p
        
        
        %Kalman Equations
        predX
        predP
        updateX
        updateK
        updateP

        %Time and Counter
        t
        j

        i_prev
       
    end
    
    methods (Access = protected)
        function setupImpl(obj)
            % Perform one-time calculations, such as computing constants
            obj.x = [0.3;0;0]; % Initial state, SoC,Vct,Vdif
            obj.p = [1e-3 0 0;0 1e-3 0;0 0 1e-3]; % Initial error covariance
            obj.k = [1; 1; 1];
            obj.j=1;

            %Update Equations
            obj.updateK = @(p,c,r) (p*c')/(c*p*c' + r);      % K[k]   = PC^T/(CPC^T + R)
            obj.updateX = @(x,k,z,g) x + k*(z-g);        % x[n|n] = x[n|n-1] + K[n](z[n] - Cx[n|n-1]-Du[n]), old: obj.updateX = @(x,k,z,g) x + k*(z-c*x-Du);
            obj.updateP = @(k,c,p) (eye(size(p)) - k*c)*p; % P[n|n] = (I-K[n]C)P[n|n-1]
            
            %Sensor Error Matrices Initialization
            obj.r = 1e-4; %some value based on measurement or something, currently from paper
            obj.q_fixed = [obj.qgain_soc*1000*obj.r 0 0;0 obj.qgain_1*0.1*obj.r 0;...
                0 0 obj.qgain_2*0.01*obj.r];
            

            %Battery Table SoC, OCV Functions
            bat_lookup = load('/Users/benjaminlevin/MATLAB/Projects/FormulaSAE/Kalman Filters/battery_lookup_lv_unique2.mat'); % Column 1 is SOC, 2 is OCV, 3 is dVdS (gradient)
            soc_lut = bat_lookup.battery_lookup(:,1);
            ocv_lut = bat_lookup.battery_lookup(:,2);
            slope_lut = bat_lookup.battery_lookup(:,3);
            obj.ke = @(soc,extrap) interp1(soc_lut, slope_lut, soc,'linear',extrap); %place/slope? on curve of OCV/SOC interpolate the slope
            obj.ocv = @(soc,extrap) interp1(soc_lut,ocv_lut, soc, 'linear',extrap);
            
            %Process Covariance Update
            obj.d_window = zeros(1,obj.N);
           
            %Output
            obj.g = @(x, ocv, I,r0) ocv - x(2) - x(3) + r0*I;

            % Time/Prediction updates;
            obj.predX = @(a,x,b,u) a*x + b*u;    %  x[n+1|n] = Ax[n] + Bu[n]
            obj.predP = @(a,p,q) a*p*a' + q;        % P[n|n]   = AP[n|n]A^T + BQB^T
            
            obj.t = 0;
            obj.i_prev = 0;
        end

        function [v1,v2,SoC, e_ekf, v, V_ocv] = stepImpl(obj, V, i,T,t_new)
            % Update Matrices
            obj.a = [1 0 0;0 exp(-(t_new-obj.t)/(obj.r_ct*obj.c_ct)) 0;...
                0 0 exp(-(t_new-obj.t)/(obj.r_dif*obj.c_dif))];
            obj.b = [(t_new-obj.t)/obj.q_nom; -1*obj.r_ct*(1-exp(-(t_new-obj.t)/(obj.r_ct*obj.c_ct)));...
                -1*obj.r_dif*(1-exp(-(t_new-obj.t)/(obj.r_dif*obj.c_dif)))];
            
            q = obj.q_fixed;  
            obj.t = t_new;
    
            
            %Predict
            obj.x = obj.predX(obj.a,obj.x,obj.b,obj.i_prev);
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
            
            obj.x(1) = max(0, min(obj.x(1),1));
            obj.c = [obj.ke(obj.x(1),extrap_ke) -1 -1];
            
            %Process Covariance Update 
            v = obj.g(obj.x,obj.ocv(obj.x(1),extrap_ocv),i,obj.r0);
            obj.d = V - v;
            e_ekf = obj.d;
            obj.i_prev = i;
            
            %Update
            obj.k = obj.updateK(obj.p, obj.c, obj.r);
            obj.x = obj.updateX(obj.x, obj.k, V, v);
            obj.p = obj.updateP(obj.k, obj.c, obj.p);
          
            obj.d_window = [obj.d_window(2:end) obj.d];
            obj.D = (1/obj.N)*sum(obj.d_window.*obj.d_window);
            obj.q_fixed = obj.k*obj.D*obj.k';
            obj.x(1) = max(0, obj.x(1));
            
            %General Output
            obj.j=obj.j+1;
            v1= obj.x(2);
            v2 = obj.x(3);
            SoC = obj.x(1);
            V_ocv = obj.ocv(SoC,extrap_ocv);
            v = obj.g(obj.x,obj.ocv(obj.x(1),extrap_ocv),obj.i_prev,obj.r0);
            
            
          
        end

        function resetImpl(~)
            
        end
    end
end

