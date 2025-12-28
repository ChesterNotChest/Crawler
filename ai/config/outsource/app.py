from flask import Flask, render_template, request, jsonify, Response, send_from_directory
import json
import time
import os
from werkzeug.utils import secure_filename
import mimetypes
import PyPDF2 # 确保 PyPDF2 已导入
import requests
import pandas as pd # 新增导入
from datetime import datetime, timedelta # 新增导入
from neo4j import GraphDatabase  # 添加Neo4j导入
import dotenv

# RAG 相关导入
from sentence_transformers import SentenceTransformer
import faiss
import numpy as np # FAISS 需要 numpy 


dotenv.load_dotenv(dotenv_path="keys.env")

# Get the absolute path of the directory where this script is located.
project_root = os.path.dirname(os.path.abspath(__file__))

# 定义上传文件保存的根目录
UPLOAD_FOLDER = os.path.join(project_root, 'uploads')
if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

app = Flask(__name__, template_folder=project_root, static_folder=os.path.join(project_root, 'static'))

# Ollama API 配置 (保持不变)
#OLLAMA_BASE_URL = "http://localhost:11434/api"
OLLAMA_BASE_URL = os.environ.get("OLLAMA_BASE_URL")
OLLAMA_HEADERS = {
    "Content-Type": "application/json"
}
#OLLAMA_MODEL = "qwen2.5:7b"
OLLAMA_MODEL = os.environ.get("OLLAMA_MODEL")



# RAG 全局变量初始化 
try:
    model_name_online = 'shibing624/text2vec-base-chinese' # Hugging Face 上的模型名称
    # 本地模型的文件夹名称应与Hugging Face上的名称匹配，或者您自定义一个并确保下载的文件在此文件夹内
    # 根据您的截图，本地路径是 local_models/shibing624/text2vec-base-chinese
    # 因此，我们拆分 model_name_online 来构建期望的本地子文件夹结构
    local_model_subpath = model_name_online.replace('/', os.sep) # 'shibing624\text2vec-base-chinese' on Windows
    local_model_full_path = os.path.join(project_root, 'local_models', local_model_subpath)

    print(f"Expected local model path: {local_model_full_path}")

    if os.path.isdir(local_model_full_path):
        print(f"Loading SentenceTransformer model from local path: {local_model_full_path}")
        embedding_model = SentenceTransformer(local_model_full_path)
    else:
        print(f"Local model not found at {local_model_full_path}.")
        print(f"Attempting to download and save '{model_name_online}' to '{local_model_full_path}'...")
        # 创建目标目录 (如果尚不存在)
        # os.makedirs(local_model_full_path, exist_ok=True) # 这行如果模型下载失败且目录已存在会引发问题，暂时注释，因为主要依赖手动放置或缓存
        
        try:
            embedding_model = SentenceTransformer(model_name_online) # 尝试下载到默认缓存或从缓存加载
            print(f"Model '{model_name_online}' loaded/downloaded successfully (likely to cache).")
            print(f"To ensure offline use and specific location, please manually ensure model files are in {local_model_full_path}")
        except Exception as download_exc:
            print(f"Failed to download or load '{model_name_online}' from Hugging Face: {download_exc}")
            print("Please ensure you have internet access and no SSL issues, or manually place the model in the local_models directory.")
            embedding_model = None

    if embedding_model:
        embedding_dim = embedding_model.get_sentence_embedding_dimension()
        print(f"Successfully initialized model. Embedding dimension: {embedding_dim}")
    else:
        print("Embedding model could not be initialized.")
        embedding_dim = 768 # Fallback dimension for shibing624/text2vec-base-chinese

except Exception as e: # 这个 except 对应第43行的 try
    print(f"An unexpected error occurred during RAG initialization: {e}")
    embedding_model = None # 标记模型加载失败
    embedding_dim = 768 # 设置一个默认值

# FAISS 索引 (内存中)
faiss_index = faiss.IndexFlatL2(embedding_dim) if embedding_model else None
# 存储原始文本块和来源信息
# 每个元素是 {'text': '文本块内容', 'source': '来源文件名 (folder/filename)'}
indexed_chunks_metadata = [] 

# 全局变量，用于存储知识数据
knowledge_data = None
# 新增：存储本地知识库中可用的学科
available_subjects = set() 

# Neo4j配置
# NEO4J_URI = "bolt://localhost:7687"
# NEO4J_USER = "neo4j"
# NEO4J_PASSWORD = "20041219"  # 请修改为您的Neo4j密码

NEO4J_URI = os.environ.get("NEO4J_URI")
NEO4J_USER = os.environ.get("NEO4J_USER")
NEO4J_PASSWORD = os.environ.get("NEO4J_PASSWORD")

class Neo4jKnowledgeGraph:
    def __init__(self, uri, user, password):
        self.driver = GraphDatabase.driver(uri, auth=(user, password))
    
    def close(self):
        self.driver.close()
    
    def clear_database(self):
        """清空数据库"""
        with self.driver.session() as session:
            session.run("MATCH (n) DETACH DELETE n")
    
    def create_knowledge_graph(self, knowledge_data):
        """根据CSV数据创建知识图谱"""
        with self.driver.session() as session:
            # 清空现有数据
            self.clear_database()
            
            # 创建约束和索引
            session.run("CREATE CONSTRAINT knowledge_point_name IF NOT EXISTS FOR (k:KnowledgePoint) REQUIRE k.name IS UNIQUE")
            session.run("CREATE CONSTRAINT subject_name IF NOT EXISTS FOR (s:Subject) REQUIRE s.name IS UNIQUE")
            session.run("CREATE CONSTRAINT grade_name IF NOT EXISTS FOR (g:Grade) REQUIRE g.name IS UNIQUE")
            
            for index, row in knowledge_data.iterrows():
                # 创建知识点节点
                session.run("""
                    MERGE (k:KnowledgePoint {name: $knowledge_point})
                    SET k.time = $time,
                        k.timestamp = datetime($time),
                        k.order = $order
                """, 
                knowledge_point=row['知识点'],
                time=row['时间'].strftime('%Y-%m-%d'),
                order=index
                )
                
                # 创建学科节点
                session.run("""
                    MERGE (s:Subject {name: $subject})
                """, subject=row['学科'])
                
                # 创建年级节点
                session.run("""
                    MERGE (g:Grade {name: $grade})
                """, grade=row['年级'])
                
                # 创建关系
                session.run("""
                    MATCH (k:KnowledgePoint {name: $knowledge_point})
                    MATCH (s:Subject {name: $subject})
                    MERGE (k)-[:BELONGS_TO]->(s)
                """, 
                knowledge_point=row['知识点'],
                subject=row['学科']
                )
                
                session.run("""
                    MATCH (k:KnowledgePoint {name: $knowledge_point})
                    MATCH (g:Grade {name: $grade})
                    MERGE (k)-[:FOR_GRADE]->(g)
                """, 
                knowledge_point=row['知识点'],
                grade=row['年级']
                )
            
            # 创建知识点之间的时序关系
            session.run("""
                MATCH (k1:KnowledgePoint), (k2:KnowledgePoint)
                WHERE k1.order + 1 = k2.order
                MERGE (k1)-[:NEXT]->(k2)
            """)
    
    def get_graph_data(self):
        """获取图谱数据用于前端展示"""
        with self.driver.session() as session:
            # 获取所有节点
            nodes_result = session.run("""
                MATCH (n)
                RETURN n, labels(n) as labels
            """)
            
            nodes = []
            for record in nodes_result:
                node = record['n']
                labels = record['labels']
                
                # 处理节点属性，转换DateTime对象为字符串
                properties = {}
                for key, value in dict(node).items():
                    if hasattr(value, 'isoformat'):  # DateTime对象
                        properties[key] = value.isoformat()
                    else:
                        properties[key] = value
                
                node_data = {
                    'id': node.element_id,
                    'name': node.get('name', ''),
                    'type': labels[0].lower() if labels else 'unknown',
                    'properties': properties
                }
                nodes.append(node_data)
            
            # 获取所有关系
            relationships_result = session.run("""
                MATCH (n1)-[r]->(n2)
                RETURN n1, r, n2, type(r) as rel_type
            """)
            
            links = []
            for record in relationships_result:
                source = record['n1']
                target = record['n2']
                rel_type = record['rel_type']
                
                # 处理关系属性，转换DateTime对象为字符串
                properties = {}
                for key, value in dict(record['r']).items():
                    if hasattr(value, 'isoformat'):  # DateTime对象
                        properties[key] = value.isoformat()
                    else:
                        properties[key] = value
                
                link_data = {
                    'source': source.element_id,
                    'target': target.element_id,
                    'type': rel_type.lower(),
                    'properties': properties
                }
                links.append(link_data)
            
            return {'nodes': nodes, 'links': links}
    
    def get_recommendations(self, current_time=None):
        """基于Neo4j查询获取学习推荐"""
        if current_time is None:
            current_time = datetime.now()
        
        current_date_str = current_time.strftime('%Y-%m-%d')
        
        with self.driver.session() as session:
            # 学习推荐：未来的知识点
            learn_result = session.run("""
                MATCH (k:KnowledgePoint)-[:BELONGS_TO]->(s:Subject)
                MATCH (k)-[:FOR_GRADE]->(g:Grade)
                WHERE k.time >= $current_date
                RETURN k.name as knowledge_point, k.time as time, 
                       s.name as subject, g.name as grade, k.order as order
                ORDER BY k.order
            """, current_date=current_date_str)
            
            learn_recommendations = []
            for record in learn_result:
                learn_recommendations.append({
                    'knowledge_point': record['knowledge_point'],
                    'time': record['time'],
                    'subject': record['subject'],
                    'grade': record['grade'],
                    'type': 'learn'
                })
            
            # 复习推荐：过去的知识点
            review_result = session.run("""
                MATCH (k:KnowledgePoint)-[:BELONGS_TO]->(s:Subject)
                MATCH (k)-[:FOR_GRADE]->(g:Grade)
                WHERE k.time < $current_date
                RETURN k.name as knowledge_point, k.time as time,
                       s.name as subject, g.name as grade,
                       duration.between(date(k.time), date($current_date)).days as days_ago
                ORDER BY k.order DESC
            """, current_date=current_date_str)
            
            review_recommendations = []
            for record in review_result:
                review_recommendations.append({
                    'knowledge_point': record['knowledge_point'],
                    'time': record['time'],
                    'subject': record['subject'],
                    'grade': record['grade'],
                    'days_ago': record['days_ago'],
                    'type': 'review'
                })
            
            return {
                'learn': learn_recommendations,
                'review': review_recommendations
            }
    
    def get_knowledge_path(self, start_point, end_point):
        """获取两个知识点之间的学习路径"""
        with self.driver.session() as session:
            result = session.run("""
                MATCH path = shortestPath((start:KnowledgePoint {name: $start})-[:NEXT*]->(end:KnowledgePoint {name: $end}))
                RETURN [node in nodes(path) | node.name] as path
            """, start=start_point, end=end_point)
            
            record = result.single()
            return record['path'] if record else []

# 初始化Neo4j连接
try:
    neo4j_graph = Neo4jKnowledgeGraph(NEO4J_URI, NEO4J_USER, NEO4J_PASSWORD)
    print("Neo4j连接成功")
except Exception as e:
    print(f"Neo4j连接失败: {e}")
    neo4j_graph = None

def load_knowledge_data():
    global knowledge_data

    #csv_name = 'knowledge_data.csv'
    csv_name = os.environ.get("CSV_NAME")
    csv_path = os.path.join(project_root, csv_name)

    if os.path.exists(csv_path):
        try:
            # 修改：跳过标题行，正确读取数据
            knowledge_data = pd.read_csv(csv_path, names=['时间', '年级', '学科', '知识点'], skiprows=1)
            knowledge_data['时间'] = pd.to_datetime(knowledge_data['时间'], format='%Y年%m月%d日')
            print(f"成功加载知识数据，共 {len(knowledge_data)} 条记录")
            
            # 将数据导入Neo4j
            if neo4j_graph:
                neo4j_graph.create_knowledge_graph(knowledge_data)
                print("知识图谱已导入Neo4j数据库")
            
            return True
        except Exception as e:
            print(f"加载知识数据失败: {e}")
            knowledge_data = None
            return False
    else:
        print(f"知识数据文件不存在: {csv_path}")
        knowledge_data = None
        return False

# 添加缺失的build_or_update_index函数
def build_or_update_index():
    """构建或更新RAG索引"""
    global faiss_index, indexed_chunks_metadata
    
    if not embedding_model:
        print("Warning: Embedding model not available. Cannot build index.")
        return False
    
    try:
        # 清空现有索引
        faiss_index = faiss.IndexFlatL2(embedding_dim)
        indexed_chunks_metadata = []
        
        # 扫描uploads文件夹中的所有PDF文件
        uploads_dir = os.path.join(project_root, 'uploads')
        if not os.path.exists(uploads_dir):
            print("Uploads directory not found.")
            return False
        
        chunk_count = 0
        for root, dirs, files in os.walk(uploads_dir):
            for file in files:
                if file.lower().endswith('.pdf'):
                    file_path = os.path.join(root, file)
                    relative_path = os.path.relpath(file_path, uploads_dir)
                    
                    try:
                        # 提取PDF文本
                        with open(file_path, 'rb') as pdf_file:
                            pdf_reader = PyPDF2.PdfReader(pdf_file)
                            text = ""
                            for page in pdf_reader.pages:
                                text += page.extract_text() + "\n"
                        
                        if text.strip():
                            # 简单分块（按段落）
                            chunks = [chunk.strip() for chunk in text.split('\n\n') if chunk.strip() and len(chunk.strip()) > 50]
                            
                            for chunk in chunks:
                                # 生成嵌入向量
                                embedding = embedding_model.encode([chunk], convert_to_tensor=False)[0]
                                embedding_np = np.array([embedding]).astype('float32')
                                
                                # 添加到FAISS索引
                                faiss_index.add(embedding_np)
                                
                                # 保存元数据
                                indexed_chunks_metadata.append({
                                    'text': chunk,
                                    'source': relative_path
                                })
                                
                                chunk_count += 1
                    
                    except Exception as e:
                        print(f"Error processing {file_path}: {e}")
                        continue
        
        print(f"Successfully built index with {chunk_count} chunks from {len([f for f in os.listdir(uploads_dir) if f.lower().endswith('.pdf')])} PDF files.")
        return True
        
    except Exception as e:
        print(f"Error building index: {e}")
        return False

@app.route('/')
def index():
    return render_template('index.html')
# 在stream_llm_response函数中，确保分隔符格式正确
def stream_llm_response(prompt, rag_context_data=None):
    try:
        # 首先发送RAG上下文数据给前端
        if rag_context_data:
            rag_context_json = json.dumps(rag_context_data, ensure_ascii=False)
            yield f"__RAG_CONTEXT_START__\n{rag_context_json}\n__RAG_CONTEXT_END__\n"  # 注意换行符
        
        # 构建Ollama API请求
        ollama_data = {
            "model": OLLAMA_MODEL,
            "prompt": prompt,
            "stream": True
        }
        
        # 发送请求到Ollama
        response = requests.post(
            f"{OLLAMA_BASE_URL}/generate",
            headers=OLLAMA_HEADERS,
            json=ollama_data,
            stream=True
        )
        
        if response.status_code == 200:
            for line in response.iter_lines():
                if line:
                    try:
                        chunk_data = json.loads(line.decode('utf-8'))
                        if 'response' in chunk_data:
                            yield chunk_data['response']
                        if chunk_data.get('done', False):
                            break
                    except json.JSONDecodeError:
                        continue
        else:
            yield f"错误：无法连接到LLM服务 (状态码: {response.status_code})"
            
    except requests.exceptions.RequestException as e:
        yield f"错误：LLM服务连接失败 - {str(e)}"
    except Exception as e:
        yield f"错误：处理LLM响应时发生异常 - {str(e)}"
# 修改 /api/search 路由以使用 RAG
@app.route('/api/search', methods=['POST'])
def search():
    data = request.get_json()
    query = data.get('query')

    if not query:
        return jsonify({'error': 'Query is required'}), 400
    
    rag_context_for_frontend = [] # 用于发送给前端的RAG上下文
    rag_results_text_for_llm = "" # 用于构建LLM prompt的RAG文本

    if not embedding_model or not faiss_index:
        print("Warning: Embedding model or FAISS index not available. Performing search without RAG.")
        rag_results_text_for_llm = "知识库未启用或初始化失败。"
    elif faiss_index.ntotal == 0:
        print("FAISS index is empty. Cannot perform search.")
        rag_results_text_for_llm = "知识库为空，无法提供参考信息。"
    else:
        try:
            print(f"User query: {query}")
            query_embedding = embedding_model.encode([query], convert_to_tensor=False)[0]
            query_embedding_np = np.array([query_embedding]).astype('float32')
            k = 3
            distances, indices = faiss_index.search(query_embedding_np, k)
            
            retrieved_chunks_for_llm = []
            
            # 在搜索函数中使用
            if len(indices[0]) > 0 and indices[0][0] != -1:
                for i, idx in enumerate(indices[0]):
                    if 0 <= idx < len(indexed_chunks_metadata):
                        chunk_data = indexed_chunks_metadata[idx]
                        
                        # 简化的相关性计算：使用距离的倒数进行归一化
                        distance = float(distances[0][i])
                        # 使用指数衰减函数将距离转换为0-1之间的相关性分数
                        relevance_score = np.exp(-distance * 2)  # 调整衰减因子以获得合适的分布
                        
                        rag_context_for_frontend.append({
                            'source': chunk_data['source'],
                            'text': chunk_data['text'],
                            'distance': distance,
                            'relevance_score': float(relevance_score)
                        })
                        retrieved_chunks_for_llm.append(chunk_data['text'])
                        print(f"  Retrieved chunk (source: {chunk_data['source']}, distance: {distance}, relevance: {relevance_score:.3f}):")
                        print(f"    '{chunk_data['text'][:100]}...'")
            
            if retrieved_chunks_for_llm:
                rag_results_text_for_llm = "\n\n[知识库检索结果]\n根据您的查询，在知识库中找到以下相关信息：\n"
                for i, chunk_text_content in enumerate(retrieved_chunks_for_llm):
                    rag_results_text_for_llm += f"- 相关片段{i+1} (来自: {rag_context_for_frontend[i]['source'] if i < len(rag_context_for_frontend) else '未知来源'}): {chunk_text_content}\n"
            else:
                rag_results_text_for_llm = "\n\n[知识库检索结果]\n在知识库中未找到与您查询直接相关的信息。"

        except Exception as e:
            print(f"Error during RAG retrieval: {e}")
            rag_results_text_for_llm = "\n\n[知识库检索提示]\n检索知识库时发生错误。"
    
    full_prompt_for_llm = f"用户问题：{query}{rag_results_text_for_llm}请结合以上信息（如果提供），详细回答用户的问题。"
    
    # 将 rag_context_for_frontend 传递给 stream_llm_response
    return Response(stream_llm_response(full_prompt_for_llm, rag_context_data=rag_context_for_frontend), content_type='text/event-stream; charset=utf-8') # 使用 text/event-stream 更适合流式响应


# 新增：获取上传目录下的文件夹列表 (保持不变)
@app.route('/api/folders', methods=['GET'])
def list_folders():
    folders = [d for d in os.listdir(UPLOAD_FOLDER) if os.path.isdir(os.path.join(UPLOAD_FOLDER, d))]
    return jsonify(folders), 200

# 修改 /api/upload 路由，在PDF上传后更新索引
@app.route('/api/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return jsonify({'error': 'No file part'}), 400
    file = request.files['file']
    target_folder_name = request.form.get('target_folder', 'default')

    if file.filename == '':
        return jsonify({'error': 'No selected file'}), 400
    
    if file:
        original_filename = os.path.basename(file.filename) 
        safe_target_folder_name = secure_filename(target_folder_name)
        if not safe_target_folder_name:
            safe_target_folder_name = 'default'

        final_upload_dir = os.path.join(UPLOAD_FOLDER, safe_target_folder_name)
        if not os.path.exists(final_upload_dir):
            os.makedirs(final_upload_dir)

        filepath = os.path.join(final_upload_dir, original_filename)
        
        try:
            file.save(filepath)
            print(f"File saved to {filepath}")

            # 如果上传的是PDF文件，则重建RAG索引
            # 注意：对于大量文件或频繁上传，这是一个昂贵的操作
            # 生产环境应考虑更优化的增量索引策略
            if original_filename.lower().endswith('.pdf') and embedding_model:
                print(f"PDF file {original_filename} uploaded. Rebuilding RAG index...")
                build_or_update_index() # 调用我们之前定义的索引构建函数
            
            file_type, _ = mimetypes.guess_type(filepath)
            if file_type is None:
                file_type = 'application/octet-stream'

            return jsonify({
                'id': f'{safe_target_folder_name}/{original_filename}',
                'name': original_filename,
                'folder': safe_target_folder_name,
                'type': file_type,
                'size': os.path.getsize(filepath),
                'uploadTime': time.time(),
                'status': 'processing' # 前端可以根据这个状态决定是否立即刷新文档列表或显示处理中
            }), 200
        except Exception as e:
            print(f"Error saving file or updating index: {e}")
            return jsonify({'error': f'Error saving file: {str(e)}'}), 500

# 获取已上传文档列表的接口 (保持不变)
@app.route('/api/documents', methods=['GET'])
def list_documents():
    documents_list = []
    for folder_name in os.listdir(UPLOAD_FOLDER):
        folder_path = os.path.join(UPLOAD_FOLDER, folder_name)
        if os.path.isdir(folder_path):
            for filename in os.listdir(folder_path):
                filepath = os.path.join(folder_path, filename)
                if os.path.isfile(filepath):
                    file_type, _ = mimetypes.guess_type(filepath)
                    if file_type is None:
                        file_type = 'application/octet-stream'
                    documents_list.append({
                        'id': f'{folder_name}/{filename}',
                        'name': filename,
                        'folder': folder_name,
                        'type': file_type,
                        'size': os.path.getsize(filepath),
                        'uploadTime': os.path.getctime(filepath),
                        'status': 'completed'
                    })
            
    return jsonify(documents_list), 200


@app.route('/api/documents/<path:doc_path>', methods=['GET'])
def get_document_content(doc_path):
    filepath = os.path.join(UPLOAD_FOLDER, doc_path)
    folder_name, filename = os.path.split(doc_path)

    if not os.path.exists(filepath) or not os.path.isfile(filepath):
        return jsonify({'error': '文档未找到'}), 404

    _, extension = os.path.splitext(filename)
    extension = extension.lower()
    
    # 获取文件大小
    file_size = os.path.getsize(filepath)
    
    try:
        # 文本文件
        if extension in ['.txt', '.md', '.py', '.js', '.html', '.css', '.json', '.xml', '.csv']:
            # 限制文本文件大小，避免内存问题
            if file_size > 1024 * 1024:  # 1MB
                return jsonify({
                    'type': 'complex', 
                    'content': f'{filename} 文件过大（{file_size/1024/1024:.1f}MB），请下载后查看。',
                    'filename': filename, 
                    'folder': folder_name
                }), 200
            
            # 尝试多种编码
            encodings = ['utf-8', 'gbk', 'gb2312', 'latin-1']
            content = None
            for encoding in encodings:
                try:
                    with open(filepath, 'r', encoding=encoding) as f:
                        content = f.read()
                    break
                except UnicodeDecodeError:
                    continue
            
            if content is None:
                return jsonify({
                    'type': 'complex',
                    'content': f'{filename} 文件编码不支持，请下载后查看。',
                    'filename': filename,
                    'folder': folder_name
                }), 200
            
            return jsonify({
                'type': 'text', 
                'content': content, 
                'filename': filename, 
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # 图片文件
        elif extension in ['.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp', '.svg']:
            return jsonify({
                'type': 'image', 
                'url': f'/uploads/{doc_path}', 
                'filename': filename, 
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # PDF文件
        elif extension == '.pdf':
            return jsonify({
                'type': 'pdf_link', 
                'url': f'/uploads/{doc_path}', 
                'filename': filename, 
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # Office文档
        elif extension in ['.docx', '.doc', '.pptx', '.ppt', '.xlsx', '.xls']:
            file_type_map = {
                '.docx': 'Word文档', '.doc': 'Word文档',
                '.pptx': 'PowerPoint演示文稿', '.ppt': 'PowerPoint演示文稿',
                '.xlsx': 'Excel表格', '.xls': 'Excel表格'
            }
            return jsonify({
                'type': 'complex', 
                'content': f'{filename} 是 {file_type_map.get(extension, "Office")} 文件，需要下载后使用相应软件打开。', 
                'filename': filename, 
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # 压缩文件
        elif extension in ['.zip', '.rar', '.7z', '.tar', '.gz']:
            return jsonify({
                'type': 'complex',
                'content': f'{filename} 是压缩文件，需要下载后解压查看。',
                'filename': filename,
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # 视频文件
        elif extension in ['.mp4', '.avi', '.mov', '.wmv', '.flv', '.mkv']:
            return jsonify({
                'type': 'complex',
                'content': f'{filename} 是视频文件，需要下载后使用视频播放器观看。',
                'filename': filename,
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # 音频文件
        elif extension in ['.mp3', '.wav', '.flac', '.aac', '.ogg']:
            return jsonify({
                'type': 'complex',
                'content': f'{filename} 是音频文件，需要下载后使用音频播放器播放。',
                'filename': filename,
                'folder': folder_name,
                'size': file_size
            }), 200
            
        # 其他未知类型
        else:
            return jsonify({
                'type': 'unknown', 
                'content': f'不支持预览 {extension} 类型的文件。文件大小：{file_size/1024:.1f}KB', 
                'filename': filename, 
                'folder': folder_name,
                'size': file_size
            }), 200
            
    except Exception as e:
        return jsonify({'error': f'无法读取或处理文档内容: {str(e)}'}), 500

# 新增：用于直接提供上传文件夹中的文件（例如图片）
@app.route('/uploads/<path:filepath>')
def serve_uploaded_file(filepath):
    return send_from_directory(UPLOAD_FOLDER, filepath)

@app.route('/api/recommendations')
def recommendations_api():
    if neo4j_graph:
        try:
            # 获取Neo4j推荐
            neo4j_recommendations = neo4j_graph.get_recommendations()
            
            # 获取文档统计信息
            folders_data = []
            
            if os.path.exists(UPLOAD_FOLDER):
                for folder_name in os.listdir(UPLOAD_FOLDER):
                    folder_path = os.path.join(UPLOAD_FOLDER, folder_name)
                    if os.path.isdir(folder_path):
                        doc_count = 0
                        total_size = 0
                        
                        for file_name in os.listdir(folder_path):
                            file_path = os.path.join(folder_path, file_name)
                            if os.path.isfile(file_path):
                                doc_count += 1
                                total_size += os.path.getsize(file_path)
                        
                        if doc_count > 0:
                            folders_data.append({
                                'name': folder_name,
                                'doc_count': doc_count,
                                'total_size': total_size
                            })
            
            # 获取当前时间和时间段信息
            now = datetime.now()
            current_hour = now.hour
            
            # 判断学习时间段
            if 6 <= current_hour < 9:
                period = {'name': '晨读时光', 'recommendation': '适合记忆和背诵'}
            elif 9 <= current_hour < 12:
                period = {'name': '上午黄金', 'recommendation': '适合学习新知识'}
            elif 14 <= current_hour < 17:
                period = {'name': '下午时光', 'recommendation': '适合练习和复习'}
            elif 19 <= current_hour < 22:
                period = {'name': '晚间学习', 'recommendation': '适合总结和思考'}
            else:
                period = {'name': '休息时间', 'recommendation': '建议适当休息'}
            
            recommendations = {
                'neo4j_recommendations': neo4j_recommendations,
                'folders_data': folders_data,
                'current_time': now.strftime('%Y-%m-%d %H:%M:%S'),
                'period': period,
                'timestamp': int(time.time())
            }
            
            return jsonify(recommendations)
        except Exception as e:
            return jsonify({'error': str(e)}), 500
    else:
        return jsonify({'error': 'Neo4j连接不可用'}), 500

@app.route('/api/knowledge_graph')
def knowledge_graph_api():
    if neo4j_graph:
        graph_data = neo4j_graph.get_graph_data()
        return jsonify(graph_data)
    else:
        return jsonify({'error': 'Neo4j连接不可用'}), 500

@app.route('/api/knowledge_path')
def knowledge_path_api():
    start_point = request.args.get('start')
    end_point = request.args.get('end')
    
    if not start_point or not end_point:
        return jsonify({'error': '需要提供起始和结束知识点'}), 400
    
    if neo4j_graph:
        path = neo4j_graph.get_knowledge_path(start_point, end_point)
        return jsonify({'path': path})
    else:
        return jsonify({'error': 'Neo4j连接不可用'}), 500

# 在应用关闭时关闭Neo4j连接
@app.teardown_appcontext
def close_neo4j(error):
    if neo4j_graph:
        neo4j_graph.close()

# 添加Flask应用启动代码
# 移动到文件末尾

# 添加时序知识图谱数据结构
class TemporalKnowledgeGraph:
    def __init__(self):
        self.nodes = {}  # 知识点节点
        self.temporal_edges = {}  # 时序关系边
        self.learning_sequences = {}  # 学习序列
    
    def add_temporal_node(self, node_id, concept, estimated_time, difficulty):
        """添加带时间信息的知识点"""
        self.nodes[node_id] = {
            'concept': concept,
            'estimated_learning_time': estimated_time,  # 预计学习时长
            'difficulty_level': difficulty,
            'prerequisites': [],
            'unlocks': [],
            'creation_time': datetime.now(),
            'last_updated': datetime.now()
        }
    
    def add_temporal_relationship(self, from_node, to_node, relation_type, time_gap=0):
        """添加时序关系"""
        edge_id = f"{from_node}-{to_node}"
        self.temporal_edges[edge_id] = {
            'from': from_node,
            'to': to_node,
            'type': relation_type,  # 'prerequisite', 'leads_to', 'parallel'
            'time_gap': time_gap,   # 学习间隔时间
            'strength': 1.0,        # 关系强度
            'created_at': datetime.now()
        }
        
        # 更新节点的前置和后续关系
        if relation_type == 'prerequisite':
            self.nodes[to_node]['prerequisites'].append(from_node)
            self.nodes[from_node]['unlocks'].append(to_node)
    
    def generate_learning_path(self, user_progress, target_concepts):
        """生成个性化学习路径"""
        # 基于用户当前进度和目标概念生成最优学习路径
        completed_concepts = user_progress.get('completed', [])
        current_time = user_progress.get('current_time', 0)
        
        # 使用拓扑排序生成学习序列
        learning_path = self.topological_sort_with_time(completed_concepts, target_concepts)
        
        return {
            'path': learning_path,
            'estimated_total_time': sum(self.nodes[node]['estimated_learning_time'] for node in learning_path),
            'milestones': self.identify_milestones(learning_path)
        }
    
    def get_timeline_data(self, current_progress):
        """获取时间轴可视化数据"""
        timeline_data = {
            'nodes': [],
            'edges': [],
            'milestones': [],
            'current_position': current_progress
        }
        
        cumulative_time = 0
        for node_id, node_data in self.nodes.items():
            timeline_data['nodes'].append({
                'id': node_id,
                'concept': node_data['concept'],
                'start_time': cumulative_time,
                'duration': node_data['estimated_learning_time'],
                'status': self.get_node_status(node_id, current_progress),
                'difficulty': node_data['difficulty_level']
            })
            cumulative_time += node_data['estimated_learning_time']
        
        return timeline_data

# 添加新的API端点
@app.route('/api/timeline', methods=['GET'])
def get_timeline_data():
    user_progress = request.args.get('progress', 0, type=int)
    timeline_data = temporal_kg.get_timeline_data(user_progress)
    return jsonify(timeline_data)

@app.route('/api/learning_path', methods=['POST'])
def generate_learning_path():
    data = request.get_json()
    user_progress = data.get('user_progress', {})
    target_concepts = data.get('target_concepts', [])
    
    learning_path = temporal_kg.generate_learning_path(user_progress, target_concepts)
    return jsonify(learning_path)

if __name__ == '__main__':
    # 在应用启动时初始化数据
    with app.app_context():
        load_knowledge_data()  # 加载知识数据到Neo4j
        build_or_update_index()  # 构建RAG索引
    
    # 启动Flask应用
    app.run(debug=True, host='0.0.0.0', port=5000, use_reloader=False)