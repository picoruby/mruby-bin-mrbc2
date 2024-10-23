MRuby::Gem::Specification.new 'mruby-bin-mrbc2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby & PicoRuby developers'
  spec.summary = 'mruby compiler executable'
  spec.add_dependency "mruby-compiler2"
  if cc.defines.include?('MRC_CUSTOM_ALLOC')
    spec.add_dependency 'picoruby-mrubyc' # For alloc.c etc.
  end
  spec.add_conflict 'mruby-compiler'
  spec.add_conflict 'mruby-bin-picorbc'

  cc.defines << %w(MRC_TARGET_PICORUBY)

  compiler2_dir = build.gems['mruby-compiler2'].dir

  cc.include_paths << "#{compiler2_dir}/lib/prism/include"

  exe_name = 'picorbc'

  exec = exefile("#{build.build_dir}/bin/#{exe_name}")

  mrbc2_objs = Dir.glob("#{dir}/tools/mrbc/*.c").map { |f| objfile(f.pathmap("#{build_dir}/tools/mrbc/%n")) }
  mrbc2_objs += build.gems['mruby-compiler2'].objs
  mrbc2_objs.delete_if{|o| o.include?("gem_init")}
  if cc.defines.include?('MRC_CUSTOM_ALLOC')
    mrubyc_dir = "#{build.gems['picoruby-mrubyc'].dir}/lib/mrubyc"
    cc.include_paths << "#{build.gems['picoruby-mrubyc'].dir}/lib/mrubyc/src"
    Dir.glob(["#{mrubyc_dir}/src/*.c", "#{mrubyc_dir}/hal/posix/*.c"]).each do |mrubyc_src|
      mrubyc_obj = objfile(mrubyc_src.pathmap("#{build.build_dir}/tools/mrbc/%n"))
      file mrubyc_obj => mrubyc_src do |f|
        # To avoid "mrbc_raw_free(): NULL pointer was given.\n"
        cc.defines << "NDEBUG"
        cc.run f.name, f.prerequisites.first
        cc.reject! { |d| d == "NDEBUG" }
      end
      mrbc2_objs << mrubyc_obj
    end
  end

  libraries = []

  file exec => mrbc2_objs do |t|
    build.linker.run t.name, t.prerequisites, libraries
  end

  build.bins << exe_name

end
