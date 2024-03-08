MRuby::Gem::Specification.new 'mruby-bin-mrbc' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby & PicoRuby developers'
  spec.summary = 'mruby compiler executable'
  spec.add_dependency "mruby-compiler2"
  spec.add_conflict 'mruby-compiler'

  spec.cc.defines << %w(MRC_TARGET_PICORUBY)

  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  exec = exefile("#{build.build_dir}/bin/mrbc2")
  mrbc2_objs = Dir.glob("#{spec.dir}/tools/mrbc/*.c").map { |f| objfile(f.pathmap("#{spec.build_dir}/tools/mrbc/%n")) }

  file exec => mrbc2_objs << build.libmruby_static do |t|
    build.linker.run t.name, t.prerequisites
  end

  build.bins << 'mrbc2'

end
