/*
╔═╗   ╔═╗ ╔═╗           ╔═╗                   ╔═╗                   ╔═╗     
║ ║   ║ ║ ╚═╝ ╔═══════╗ ║ ╚═════╗   ╔═════╗ ╔═╝ ╚═╗   ╔═══════╗   ╔═╝ ╚═╗   
║ ╚═══╝ ║ ╔═╗ ║ ╔═══╗ ║ ║ ╔═══╗ ║ ╔═╝   ╔═╝ ╚═╗ ╔═╝   ║ ╔═══╗ ║   ╚═╗ ╔═╝   
║ ╔═══╗ ║ ║ ║ ║ ║   ║ ║ ║ ║   ║ ║ ╚═══╗ ╚═╗   ║ ║     ║ ║   ║ ║     ║ ║     
║ ║   ║ ║ ║ ║ ║ ╚═══╝ ║ ║ ║   ║ ║ ╔═══╝ ╔═╝   ║ ╚═══╗ ║ ╚═══╝ ╚═╗   ║ ╚═══╗ 
╚═╝   ╚═╝ ╚═╝ ╚═════╗ ║ ╚═╝   ╚═╝ ╚═════╝     ╚═════╝ ╚═════════╝   ╚═════╝ 
                ╔═══╝ ║                                                     
                ╚═════╝                                                     
                © Copyright 2026 
            `https://github.com/highstat/BW16-Repeater` 
    Name: BuildConfig 
    Description: Build-time feature flags 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#pragma once
#ifndef HAVE_BLE
#define HAVE_BLE 0
#endif
