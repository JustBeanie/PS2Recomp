include(CheckIPOSupported)

check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

# Whole-program optimization defers all codegen to a mostly single-threaded
# link. With a full game's generated code that link can take hours, so turn
# it off while iterating on link/boot errors.
option(PS2X_ENABLE_LTO "Whole-program optimization (/GL + /LTCG, IPO) for Release" ON)

function(EnableFastReleaseMode TargetName)
    message("> Enabling optimization for: ${TargetName} (LTO=${PS2X_ENABLE_LTO})")
    if(MSVC)
        if(PS2X_ENABLE_LTO)
            target_compile_options(${TargetName} PRIVATE $<$<CONFIG:Release>:/GL>)
            target_link_options(${TargetName} PRIVATE $<$<CONFIG:Release>:/LTCG>)
        endif()

        target_compile_options(${TargetName} PRIVATE
            $<$<CONFIG:Release>:
                /O2 # speed
                /Ob2 # inline aggressively
                /Oi # intrinsics
                /Gy # function-level linking
                /Gw # global data in COMDAT
                /GF # string pooling
                /Zc:inline # remove unreferenced inline
                /fp:fast # fast math (graphics friendly)
                /DNDEBUG
                /arch:AVX2 # Advanced Vector Extensions 2
                /GS- # Disable Buffer Security Check (faster)
                /Qspectre- # Disable Spectre mitigations (faster)
            >
        )

        if(TARGET ${TargetName})
            target_link_options(${TargetName} PRIVATE
                $<$<CONFIG:Release>:
                    /OPT:REF # remove unreferenced
                    /OPT:ICF # fold identical COMDATs
                >
            )
        endif()
    endif()

    if(NOT PS2X_ENABLE_LTO)
        set_property(TARGET ${TargetName} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
    elseif(IPO_SUPPORTED)
        set_property(TARGET ${TargetName} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(WARNING "Interprocedural optimization not supported: ${IPO_ERROR}")
    endif()
endfunction()