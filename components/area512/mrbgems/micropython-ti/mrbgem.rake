MRuby::Gem::Specification.new('micropython-ti') do |spec|
  spec.license = 'MIT'
  spec.author = 'hamachan'
  spec.summary = 'MicroPython on-device completion engine'

  spec.add_dependency 'picoruby-ti'
  spec.cc.include_paths << "#{spec.dir}/include"
  spec.cc.include_paths << "#{spec.dir}/src"
  spec.cc.include_paths << "#{spec.dir}/src/base"
  spec.cc.include_paths << "#{spec.dir}/src/builtin"
  spec.cc.include_paths << "#{spec.dir}/src/context"
  spec.cc.include_paths << "#{spec.dir}/src/diagnostic"
  spec.cc.include_paths << "#{spec.dir}/src/eval"
  spec.cc.include_paths << "#{spec.dir}/src/eval/method_evaluator"
  spec.cc.include_paths << "#{spec.dir}/src/generated"
  spec.cc.include_paths << "#{spec.dir}/src/hover"
  spec.cc.include_paths << "#{spec.dir}/src/suggest"
  spec.cc.include_paths << "#{spec.dir}/../picoruby-ti/src/base"
  spec.cc.include_paths << "#{spec.dir}/../../../area512_tree_sitter/tree-sitter/lib/include"
  spec.cc.include_paths << "#{spec.dir}/../../../area512_tree_sitter/tree-sitter-python/bindings/c"

  extensions = spec.compilers.flat_map { |compiler| compiler.source_exts } * ","
  spec.objs = Dir["#{spec.dir}/src/**/*{#{extensions}}"]
    .map do |source|
      relative_path = source.relative_path_from(spec.dir).to_s
      spec.objfile(relative_path.pathmap("#{spec.build_dir}/%X"))
    end
end
