MRuby::Gem::Specification.new 'mruby-bin-mrbc2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby & PicoRuby developers'
  spec.summary = 'mruby compiler executable'

  spec.add_dependency "mruby-compiler2"
  spec.add_conflict 'mruby-compiler'
  spec.add_conflict 'mruby-bin-picorbc'

  compiler2_dir = build.gems['mruby-compiler2'].dir

  cc.include_paths << "#{compiler2_dir}/lib/prism/include"

  mrbc2_objs = Dir.glob("#{dir}/tools/mrbc/*.c").map { |f| objfile(f.pathmap("#{build_dir}/tools/mrbc/%n")) }
  mrbc2_objs += build.gems['mruby-compiler2'].objs
  mrbc2_objs.delete_if{|o| o.include?("gem_init")}

  if cc.defines.include?('PICORB_VM_MRUBYC')
    spec.add_dependency 'picoruby-mrubyc' # For alloc.c etc.
    mrubyc_dir = "#{build.gems['picoruby-mrubyc'].dir}/lib/mrubyc"
    cc.include_paths << "#{build.gems['picoruby-mrubyc'].dir}/lib/mrubyc/src"
    Dir.glob(["#{mrubyc_dir}/src/*.c", "#{mrubyc_dir}/hal/posix/*.c"]).each do |mrubyc_src|
      mrubyc_obj = objfile(mrubyc_src.pathmap("#{build.build_dir}/tools/mrbc/%n"))
      file mrubyc_obj => mrubyc_src do |f|
        # To avoid "mrbc_raw_free(): NULL pointer was given.\n"
        cc.defines << "NDEBUG"
        cc.run f.name, f.prerequisites.first
        cc.defines.reject! { |d| d == "NDEBUG" }
      end
      mrbc2_objs << mrubyc_obj
    end
  elsif cc.defines.include?('PICORB_VM_MRUBY')
    cc.defines << 'MRB_NO_PRESYM'
    spec.add_dependency 'picoruby-mruby'
    cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/include"
    cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/lib/mruby/include"
    mruby_dir = "#{build.gems['picoruby-mruby'].dir}/lib/mruby"
    Dir.glob(["#{mruby_dir}/src/*.c"]).each do |mruby_src|
      mruby_obj = objfile(mruby_src.pathmap("#{build.build_dir}/tools/mrbc/%n"))
      file mruby_obj => mruby_src do |f|
        # To avoid "mrbc_raw_free(): NULL pointer was given.\n"
        cc.defines << "NDEBUG"
        cc.run f.name, f.prerequisites.first
        cc.defines.reject! { |d| d == "NDEBUG" }
      end
      mrbc2_objs << mruby_obj
    end
  end

  exe_name = 'picorbc'

  file exefile("#{build.build_dir}/bin/#{exe_name}") => mrbc2_objs do |t|
    build.linker.run t.name, t.prerequisites
  end

  build.bins << exe_name

end
