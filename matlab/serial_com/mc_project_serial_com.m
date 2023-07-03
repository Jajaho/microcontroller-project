% Author: Jakob Holz
% Date: 01.07.23
% Info: Script for extracting data from serial port.
% Reset microcontroller before rerunning the script.

clear;
clc;

NOP = 360;
msg = '';
debugString = "";

column = 1;
row = 1;
data = zeros(4, NOP);


% ------ Serial COM establishment ------
ports = serialportlist("available");
[m,nPorts] = size(ports);
if nPorts > 1
    warning("Multiple COM ports detected. Ensure the correct one is used!")
end
port = serialport(ports(1,1), 9600);    % Select first detected COM port
disp("Connection succesful.")

write(port,"s","char");

while 1
    if port.NumBytesAvailable > 0 
        msg = read(port, 1, "char");
        disp(msg);
        debugString = append(debugString, msg);
        if isequal(msg, ';')    % Start of first data array
            while 1
                if isequal(msg, '$')
                    return
                end
                if port.NumBytesAvailable > 0 
                    msg = read(port, 1, "char");
                    disp(msg);
                    if isequal(msg, ',')
                        column = column + 1;
                    elseif isequal(msg, ';') % Start of next data array
                        row = row + 1;
                        column = 0;
                    elseif isstrprop(msg, 'digit')
                        data(row, column) = data(row, column) + str2double(msg);
                    end
                end
            end
        end
    end
    disp('Waiting for data.')
    pause(0.005);
end




% while 1 
%     data = 'no_data';
%     data = read(port, 1000, "char");
%     if isequal(data, 'PeakMeas;')        
%         for k = 1:NOP
%             PeakMeas(k) = str2double(readline(port));
%             break
%         end
%     end
% end



%while 1
%    data = realine(port);
%    if data ~= ""
%        break
%    else
        
%    end
%end

%data75 = read(port,349114,"char");