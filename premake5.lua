-- GameTemplate Workspace Premake5 Script
-- This workspace script generates the full GameTemplate solution with Game project

workspace "GameTemplate"
    configurations { "Debug", "Release" }
    platforms { "x64", "Win32" }
    
    -- Default to x64 platform
    filter "platforms:x64"
        architecture "x64"
    filter "platforms:Win32"  
        architecture "x32"
    filter {}
    
    -- Workspace-wide settings
    startproject "Game"
    systemversion "10.0"
    
    -- Output directories
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    
    -- Define vendor projects as external projects
    externalproject "DirectXTex"
        location "Engine/vendor/DirectXTex"
        filename "DirectXTex_Desktop_2022_Win10"
        kind "StaticLib"
        uuid "371B9FA9-4C90-4AC6-A123-ACED756D6C77"
        
    externalproject "imgui"
        location "Engine/vendor/imgui"
        filename "imgui"
        kind "StaticLib"
        uuid "05A9737C-E682-451F-A307-4B01DEB9D44B"

-- Engine Library Project
project "Engine"
    location "Engine/"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    
    targetdir "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\$(ProjectName)\\"
    objdir "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\$(ProjectName)\\immd\\"
    
    -- Character Set
    characterset "Unicode"
    
    -- Windows Target Platform
    systemversion "10.0"
    
    -- Public Include Directory
    externalincludedirs {
        "%{prj.location}",
        "%{prj.location}/include"
    }

    -- Include Directories
    includedirs {
        "%{prj.location}/vendor/assimp/include",
        "%{prj.location}/vendor/imgui",
        "%{prj.location}/vendor/DirectXTex"
    }
    
    
    -- Header Files
    files {
        -- Public Headers (include directory)
        "Engine/include/DebugUI.hpp",
        "Engine/include/IGame.hpp",
        "Engine/include/Line.hpp",
        "Engine/include/Math/Easing.h",
        "Engine/include/Math/Easing.hpp",
        "Engine/include/Math/MathUtils.h",
        "Engine/include/Math/MathUtils.hpp",
        "Engine/include/Math/Matrix.h",
        "Engine/include/Math/Matrix.hpp",
        "Engine/include/Math/Quaternion.hpp",
        "Engine/include/Math/Vector2.hpp",
        "Engine/include/Math/Vector3.hpp",
        "Engine/include/Math/Vector4.hpp",
        "Engine/include/Math/Transform.hpp",
        "Engine/include/Utils.hpp",
        "Engine/include/Log.hpp",
        "Engine/include/Framework.hpp",
        "Engine/include/Input.hpp",
        "Engine/include/Model.hpp",
        "Engine/include/IScene.hpp",
        "Engine/include/Factory/AbstractSceneFactory.hpp",
        "Engine/include/Pattern/Singleton.hpp",
        "Engine/include/Sprite.hpp",
        
        -- Internal Headers (src directory)
        "Engine/src/Animation/Animation.hpp",
        "Engine/src/Animation/KeyFrame.hpp",
        "Engine/src/Animation/NodeAnimation.hpp",
        "Engine/src/Camera/Camera.hpp",
        "Engine/src/Camera/Manager/CameraManager.hpp",
        "Engine/src/Common/Common.hpp",
        "Engine/src/Config/Config.hpp",
        "Engine/src/Container/Material.hpp",
        "Engine/src/DirectX/DirectXAdapter.hpp",
        "Engine/src/DirectX/FrameRate/FrameRateLimiter.hpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/BlendMode.hpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/InputLayout.hpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/PipelineStateObject.hpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/RootSignature.hpp",
        "Engine/src/DirectX/Heap/Heap.hpp",
        "Engine/src/DirectX/Heap/SRVManager.h",
        "Engine/src/DirectX/LeakChecker/D3DResourceLeakChecker.hpp",
        "Engine/src/DirectX/Resource/DX12Resource.hpp",
        "Engine/src/DirectX/Shader/Shader.h",
        "Engine/src/Json/Json.hpp",
        "Engine/src/Light/DirectionalLight/DirectionalLight.h",
        "Engine/src/Light/LightManager.hpp",
        "Engine/src/Light/LightType.hpp",
        "Engine/src/Light/PointLight/PointLight.h",
        "Engine/src/Light/RawLight.h",
        "Engine/src/Light/SpotLight/SpotLight.h",
        "Engine/src/Line/Common/LineCommon.hpp",
        "Engine/src/Stage/LevelEditor.hpp",
        "Engine/src/Stage/Loader/StageLoader.hpp",
        "Engine/src/Stage/LevelData.hpp",
        "Engine/src/Stage/StageRepository.hpp",
        "Engine/src/Mesh/Data/MeshData.hpp",
        "Engine/src/Mesh/Mesh.hpp",
        "Engine/src/Mesh/Repository/MeshRepository.hpp",
        "Engine/src/Model/Loader/GltfLoader.hpp",
        "Engine/src/Model/Node/Node.hpp",
        "Engine/src/Model/Skeleton/Skeleton.hpp",
        "Engine/src/Model/Common/ModelCommon.hpp",
        "Engine/src/Model/Data/ModelData.hpp",
        "Engine/src/Model/Loader/IModelLoader.hpp",
        "Engine/src/Model/Loader/ObjLoader.hpp",
        "Engine/src/Model/Repository/ModelRepository.hpp",
        "Engine/src/Platform/WinApp.hpp",
        "Engine/src/PostProcess/BoxBlur/BoxBlur.hpp",
        "Engine/src/PostProcess/Executor/PostProcessExecutor.hpp",
        "Engine/src/PostProcess/Grayscale/Grayscale.hpp",
        "Engine/src/PostProcess/IPostEffect.hpp",
        "Engine/src/PostProcess/Vignette/Vignette.hpp",
        "Engine/src/Renderer/Renderer.hpp",
        "Engine/src/ResourceRepository/ResourceRepository.hpp",
        "Engine/src/Scene/SceneSwitcher.hpp",
        "Engine/src/Scheduler/Scheduler.hpp",
        "Engine/src/Sky/Common/SkyCommon.hpp",
        "Engine/src/Sky/Skybox.hpp",
        "Engine/src/Sprite/Common/SpriteCommon.hpp",
        "Engine/src/Texture/TextureManager.hpp",
        "Engine/src/Time/Time.hpp",
        "Engine/src/Timer/Timer.hpp",
        "Engine/src/Window/Window.hpp",
        
        -- Vendor Headers
        "Engine/vendor/assimp/include/assimp/AssertHandler.h",
        "Engine/vendor/json/json.hpp",
        "Engine/vendor/MagicEnum/magic_enum.hpp",
        
        -- Source Files
        "Engine/src/Camera/Camera.cpp",
        "Engine/src/Camera/Manager/CameraManager.cpp",
        "Engine/src/Common/Common.cpp",
        "Engine/src/Config/Config.cpp",
        "Engine/src/Container/Material.cpp",
        "Engine/src/Debug/DebugUI.cpp",
        "Engine/src/DirectX/DirectXAdapter.cpp",
        "Engine/src/DirectX/FrameRate/FrameRateLimiter.cpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/InputLayout.cpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/PipelineStateObject.cpp",
        "Engine/src/DirectX/GraphicsPipeline/Object/RootSignature.cpp",
        "Engine/src/DirectX/Heap/Heap.cpp",
        "Engine/src/DirectX/Heap/SRVManager.cpp",
        "Engine/src/DirectX/LeakChecker/D3DResourceLeakChecker.cpp",
        "Engine/src/DirectX/Resource/DX12Resource.cpp",
        "Engine/src/DirectX/Shader/Shader.cpp",
        "Engine/src/Framework/Framework.cpp",
        "Engine/src/Game/IGame.cpp",
        "Engine/src/Input/Input.cpp",
        "Engine/src/Json/Json.cpp",
        "Engine/src/Light/DirectionalLight/DirectionalLight.cpp",
        "Engine/src/Light/LightManager.cpp",
        "Engine/src/Light/PointLight/PointLight.cpp",
        "Engine/src/Light/SpotLight/SpotLight.cpp",
        "Engine/src/Line/Common/LineCommon.cpp",
        "Engine/src/Line/Line.cpp",
        "Engine/src/Stage/LevelEditor.cpp",
        "Engine/src/Stage/Loader/StageLoader.cpp",
        "Engine/src/Math/Easing.cpp",
        "Engine/src/Math/MathUtils.cpp",
        "Engine/src/Math/Vector2.cpp",
        "Engine/src/Math/Vector3.cpp",
        "Engine/src/Mesh/Mesh.cpp",
        "Engine/src/Mesh/Repository/MeshRepository.cpp",
        "Engine/src/Model/Loader/GltfLoader.cpp",
        "Engine/src/Model/Common/ModelCommon.cpp",
        "Engine/src/Model/Data/ModelData.cpp",
        "Engine/src/Model/Loader/ObjLoader.cpp",
        "Engine/src/Model/Model.cpp",
        "Engine/src/Model/Repository/ModelRepository.cpp",
        "Engine/src/Platform/WinApp.cpp",
        "Engine/src/PostProcess/BoxBlur/BoxBlur.cpp",
        "Engine/src/PostProcess/Executor/PostProcessExecutor.cpp",
        "Engine/src/PostProcess/Grayscale/Grayscale.cpp",
        "Engine/src/PostProcess/IPostEffect.cpp",
        "Engine/src/PostProcess/Vignette/Vignette.cpp",
        "Engine/src/Renderer/Renderer.cpp",
        "Engine/src/ResourceRepository/ResourceRepository.cpp",
        "Engine/src/Scene/IScene.cpp",
        "Engine/src/Scene/SceneSwitcher.cpp",
        "Engine/src/Scheduler/Scheduler.cpp",
        "Engine/src/Sky/Common/SkyCommon.cpp",
        "Engine/src/Sky/Skybox.cpp",
        "Engine/src/Sprite/Common/SpriteCommon.cpp",
        "Engine/src/Sprite/Sprite.cpp",
        "Engine/src/Stage/StageRepository.cpp",
        "Engine/src/System/Log.cpp",
        "Engine/src/System/Singleton/Singleton.cpp",
        "Engine/src/Texture/TextureManager.cpp",
        "Engine/src/Time/Time.cpp",
        "Engine/src/Timer/Timer.cpp",
        "Engine/src/Utils/Utils.cpp",
        "Engine/src/Window/Window.cpp",
        
        -- Additional Files
        "Engine/ClassDiagram.cd"
    }
    
    -- Additional Library Directories
    libdirs {
        "%{prj.location}/vendor/assimp/lib/%{cfg.buildcfg}",
        "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\DirectXTex",
        "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\imgui"
    }
    
    -- Project Dependencies
    dependson {
        "DirectXTex",
        "imgui"
    }
    
    -- Linker Settings
    links {
        "DirectXTex",
        "imgui"
    }
    
    -- Preprocessor Definitions
    defines {
        "_LIB",
        "NDEBUG"
    }
    
    -- Configuration-specific settings
    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        runtime "Debug"
        symbols "On"
        optimize "Off"
        
        -- Debug-specific library dependencies
        links {
            "assimp-vc143-mtd.lib"
        }
        
        -- Debug-specific compiler settings
        buildoptions {
            "/utf-8",
            "/MP",  -- Multi-processor compilation
            "/WX",  -- Treat warnings as errors
            "/W4"   -- Warning level 4
        }
        
        -- Debug runtime library
        staticruntime "On"
        
        -- Struct alignment for GPU data
        buildoptions { "/Zp16" }
        
    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "Size"  -- MinSpace optimization
        symbols "On"
        
        -- Release-specific library dependencies
        links {
            "assimp-vc143-mt.lib"
        }
        
        -- Release-specific compiler settings
        buildoptions {
            "/utf-8",
            "/MP",  -- Multi-processor compilation
            "/WX",  -- Treat warnings as errors
            "/W4"   -- Warning level 4
        }
        
        -- Release runtime library
        staticruntime "On"
        
    filter "platforms:Win32"
        defines { "WIN32" }
        
    filter "platforms:x64"
        -- x64 specific settings
        
    filter "system:windows"
        systemversion "latest"
        defines {
            "_WINDOWS",
            "_UNICODE",  
            "UNICODE"
        }
        
        -- Windows-specific build options
        buildoptions {
            "/permissive-",  -- Standards conformance
            "/Zc:preprocessor"  -- Standards-conforming preprocessor
        }
        
    filter {}  -- Reset filter

-- Game Application Project
project "Game"
    location "Game/"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    
    targetdir "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\$(ProjectName)\\"
    objdir "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\$(ProjectName)\\immd\\"
    debugdir "$(SolutionDir)"
    
    -- Character Set
    characterset "Unicode"
    
    -- Windows Target Platform
    systemversion "10.0"
    
    -- Include Directories
    includedirs {
        "Engine/include",
        "Engine/src",
        "Engine/vendor/assimp/include",
        "Engine/vendor/imgui",
        "Engine/vendor/DirectXTex"
    }
    
    -- Source Files  
    files {
        "Game/main.cpp",
        "Game/MyGame.cpp",
        "Game/MyGame.hpp",
        "Game/SampleScene.cpp",
        "Game/SampleScene.hpp",
        "Game/SceneFactory.cpp",
        "Game/SceneFactory.hpp"
    }
    
    -- Project Dependencies
    dependson {
        "Engine"
    }
    
    -- Link with Engine library
    links {
        "Engine"
    }
    
    -- Additional Include Directories for VC++ Directories
    externalincludedirs {
        "Engine",
        "Engine/include"
    }
    
    -- Additional Library Directories
    libdirs {
        "$(SolutionDir)out\\$(Platform)\\$(Configuration)\\Engine"
    }
    
    -- Project References (for Visual Studio)
    vpaths {
        ["*"] = "Game"
    }
    
    -- Configuration-specific settings
    filter "configurations:Debug"
        defines { "_DEBUG", "_WINDOWS" }
        runtime "Debug"
        symbols "On"
        optimize "Off"
        debugformat "c7"  -- Program Database (/Zi)
        warnings "High"  -- Equivalent to /W4
        fatalwarnings { "All" }  -- Treat warnings as errors (/WX)
        
        -- Debug-specific compiler settings
        buildoptions {
            "/utf-8",
            "/MP"   -- Multi-processor compilation
        }
        
        -- Debug runtime library
        staticruntime "On"
        
        -- Linker options for Debug
        linkoptions {
            "/ignore:4049",
            "/ignore:4098", 
            "/ignore:4075",
            "/ignore:4099"
        }
        
        -- Fast Link Time Code Generation for Debug
        linktimeoptimization "On"
        
        -- Post-build events (copy DLLs and Assets)
        postbuildcommands {
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "$(TargetDir)dxcompiler.dll"',
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "$(TargetDir)dxil.dll"',
            'copy "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dbg_dynamic-x86_64.dll" "$(TargetDir)clang_rt.asan_dbg_dynamic-x86_64.dll"',
            'copy "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "$(TargetDir)clang_rt.asan_dynamic-x86_64.dll"'
        }
        
        -- Pre-build events (copy Assets)
        prebuildcommands {
            'xcopy /y /s /i "$(SolutionDir)Engine\\Assets\\*" "$(SolutionDir)Assets\\"'
        }
        
    filter "configurations:Release"
        defines { "NDEBUG", "_WINDOWS" }
        runtime "Release"
        optimize "Size"  -- MinSpace optimization
        symbols "On"
        functionlevellinking "On"
        intrinsics "On"
        warnings "High"  -- Equivalent to /W4
        fatalwarnings { "All" }  -- Treat warnings as errors (/WX)
        
        -- Release-specific compiler settings
        buildoptions {
            "/utf-8",
            "/MP"   -- Multi-processor compilation
        }
        
        -- Release runtime library
        staticruntime "On"
        
        -- Linker options for Release
        linkoptions {
            "/ignore:4049",
            "/ignore:4098"
        }
        
        -- Post-build events (copy DLLs and Assets)
        postbuildcommands {
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "$(TargetDir)dxcompiler.dll"',
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "$(TargetDir)dxil.dll"',
            'copy "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dbg_dynamic-x86_64.dll" "$(TargetDir)clang_rt.asan_dbg_dynamic-x86_64.dll"',
            'copy "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "$(TargetDir)clang_rt.asan_dynamic-x86_64.dll"',
            'xcopy "$(SolutionDir)Assets\\*" "$(TargetDir)Assets\\"',
            'xcopy "$(SolutionDir)Engine\\Assets\\*" "$(TargetDir)Engine\\Assets\\"'
        }
        
        -- Pre-build events (copy Assets)
        prebuildcommands {
            'xcopy /y /s /i "$(SolutionDir)Engine\\Assets\\*" "$(SolutionDir)Assets\\"'
        }
        
    filter "platforms:Win32"
        defines { "WIN32" }
        
    filter "platforms:x64" 
        -- x64 specific settings
        
    filter "system:windows"
        systemversion "latest"
        defines {
            "_WINDOWS",
            "_UNICODE",  
            "UNICODE"
        }
        
        -- Windows-specific build options
        buildoptions {
            "/permissive-",  -- Standards conformance
            "/Zc:preprocessor"  -- Standards-conforming preprocessor
        }
        
    filter {}  -- Reset filter