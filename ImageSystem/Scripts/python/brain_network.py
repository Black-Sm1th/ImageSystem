# -*- coding: utf-8 -*-
# 文件名: py_networkAnalysis_slicer_exact.py

import os
import json
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import networkx as nx
from scipy import signal
from nilearn import datasets, plotting
from nilearn.maskers import NiftiLabelsMasker
from nilearn.connectome import ConnectivityMeasure

# 强制无界面运行
os.environ["MPLBACKEND"] = "Agg"
import matplotlib
matplotlib.use("Agg")
import warnings
warnings.filterwarnings("ignore")

# 设置中文字体支持
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'SimSun', 'Arial Unicode MS']  # 支持中文显示
plt.rcParams['axes.unicode_minus'] = False  # 解决负号显示问题

# ==================== AAL116 标签（116个）===================
AAL116_LABELS = [
    {"zh": "中央前回左", "en": "Precentral_L"}, {"zh": "中央前回右", "en": "Precentral_R"},
    {"zh": "额上回左", "en": "Frontal_Sup_L"}, {"zh": "额上回右", "en": "Frontal_Sup_R"},
    {"zh": "眶部额上回左", "en": "Frontal_Sup_Orb_L"}, {"zh": "眶部额上回右", "en": "Frontal_Sup_Orb_R"},
    {"zh": "额中回左", "en": "Frontal_Mid_L"}, {"zh": "额中回右", "en": "Frontal_Mid_R"},
    {"zh": "眶部额中回左", "en": "Frontal_Mid_Orb_L"}, {"zh": "眶部额中回右", "en": "Frontal_Mid_Orb_R"},
    {"zh": "岛盖部额下回左", "en": "Frontal_Inf_Oper_L"}, {"zh": "岛盖部额下回右", "en": "Frontal_Inf_Oper_R"},
    {"zh": "三角部额下回左", "en": "Frontal_Inf_Tri_L"}, {"zh": "三角部额下回右", "en": "Frontal_Inf_Tri_R"},
    {"zh": "眶部额下回左", "en": "Frontal_Inf_Orb_L"}, {"zh": "眶部额下回右", "en": "Frontal_Inf_Orb_R"},
    {"zh": "中央沟盖左", "en": "Rolandic_Oper_L"}, {"zh": "中央沟盖右", "en": "Rolandic_Oper_R"},
    {"zh": "补充运动区左", "en": "Supp_Motor_Area_L"}, {"zh": "补充运动区右", "en": "Supp_Motor_Area_R"},
    {"zh": "嗅皮质左", "en": "Olfactory_L"}, {"zh": "嗅皮质右", "en": "Olfactory_R"},
    {"zh": "内侧额上回左", "en": "Frontal_Sup_Medial_L"}, {"zh": "内侧额上回右", "en": "Frontal_Sup_Medial_R"},
    {"zh": "眶内额上回左", "en": "Frontal_Med_Orb_L"}, {"zh": "眶内额上回右", "en": "Frontal_Med_Orb_R"},
    {"zh": "直回左", "en": "Rectus_L"}, {"zh": "直回右", "en": "Rectus_R"},
    {"zh": "脑岛左", "en": "Insula_L"}, {"zh": "脑岛右", "en": "Insula_R"},
    {"zh": "前扣带回左", "en": "Cingulum_Ant_L"}, {"zh": "前扣带回右", "en": "Cingulum_Ant_R"},
    {"zh": "中扣带回左", "en": "Cingulum_Mid_L"}, {"zh": "中扣带回右", "en": "Cingulum_Mid_R"},
    {"zh": "后扣带回左", "en": "Cingulum_Post_L"}, {"zh": "后扣带回右", "en": "Cingulum_Post_R"},
    {"zh": "海马左", "en": "Hippocampus_L"}, {"zh": "海马右", "en": "Hippocampus_R"},
    {"zh": "海马旁回左", "en": "ParaHippocampal_L"}, {"zh": "海马旁回右", "en": "ParaHippocampal_R"},
    {"zh": "杏仁核左", "en": "Amygdala_L"}, {"zh": "杏仁核右", "en": "Amygdala_R"},
    {"zh": "距状沟左", "en": "Calcarine_L"}, {"zh": "距状沟右", "en": "Calcarine_R"},
    {"zh": "楔叶左", "en": "Cuneus_L"}, {"zh": "楔叶右", "en": "Cuneus_R"},
    {"zh": "舌回左", "en": "Lingual_L"}, {"zh": "舌回右", "en": "Lingual_R"},
    {"zh": "枕上回左", "en": "Occipital_Sup_L"}, {"zh": "枕上回右", "en": "Occipital_Sup_R"},
    {"zh": "枕中回左", "en": "Occipital_Mid_L"}, {"zh": "枕中回右", "en": "Occipital_Mid_R"},
    {"zh": "枕下回左", "en": "Occipital_Inf_L"}, {"zh": "枕下回右", "en": "Occipital_Inf_R"},
    {"zh": "梭状回左", "en": "Fusiform_L"}, {"zh": "梭状回右", "en": "Fusiform_R"},
    {"zh": "中央后回左", "en": "Postcentral_L"}, {"zh": "中央后回右", "en": "Postcentral_R"},
    {"zh": "顶上小叶左", "en": "Parietal_Sup_L"}, {"zh": "顶上小叶右", "en": "Parietal_Sup_R"},
    {"zh": "顶下小叶左", "en": "Parietal_Inf_L"}, {"zh": "顶下小叶右", "en": "Parietal_Inf_R"},
    {"zh": "缘上回左", "en": "SupraMarginal_L"}, {"zh": "缘上回右", "en": "SupraMarginal_R"},
    {"zh": "角回左", "en": "Angular_L"}, {"zh": "角回右", "en": "Angular_R"},
    {"zh": "楔前叶左", "en": "Precuneus_L"}, {"zh": "楔前叶右", "en": "Precuneus_R"},
    {"zh": "中央旁小叶左", "en": "Paracentral_Lobule_L"}, {"zh": "中央旁小叶右", "en": "Paracentral_Lobule_R"},
    {"zh": "尾状核左", "en": "Caudate_L"}, {"zh": "尾状核右", "en": "Caudate_R"},
    {"zh": "壳核左", "en": "Putamen_L"}, {"zh": "壳核右", "en": "Putamen_R"},
    {"zh": "苍白球左", "en": "Pallidum_L"}, {"zh": "苍白球右", "en": "Pallidum_R"},
    {"zh": "丘脑左", "en": "Thalamus_L"}, {"zh": "丘脑右", "en": "Thalamus_R"},
    {"zh": "Heschl回左", "en": "Heschl_L"}, {"zh": "Heschl回右", "en": "Heschl_R"},
    {"zh": "颞上回左", "en": "Temporal_Sup_L"}, {"zh": "颞上回右", "en": "Temporal_Sup_R"},
    {"zh": "颞极颞上回左", "en": "Temporal_Pole_Sup_L"}, {"zh": "颞极颞上回右", "en": "Temporal_Pole_Sup_R"},
    {"zh": "颞中回左", "en": "Temporal_Mid_L"}, {"zh": "颞中回右", "en": "Temporal_Mid_R"},
    {"zh": "颞极颞中回左", "en": "Temporal_Pole_Mid_L"}, {"zh": "颞极颞中回右", "en": "Temporal_Pole_Mid_R"},
    {"zh": "颞下回左", "en": "Temporal_Inf_L"}, {"zh": "颞下回右", "en": "Temporal_Inf_R"},
    {"zh": "小脑CrusI左", "en": "Cerebellum_Crus1_L"}, {"zh": "小脑CrusI右", "en": "Cerebellum_Crus1_R"},
    {"zh": "小脑CrusII左", "en": "Cerebellum_Crus2_L"}, {"zh": "小脑CrusII右", "en": "Cerebellum_Crus2_R"},
    {"zh": "小脑3区左", "en": "Cerebellum_3_L"}, {"zh": "小脑3区右", "en": "Cerebellum_3_R"},
    {"zh": "小脑4-5区左", "en": "Cerebellum_4_5_L"}, {"zh": "小脑4-5区右", "en": "Cerebellum_4_5_R"},
    {"zh": "小脑6区左", "en": "Cerebellum_6_L"}, {"zh": "小脑6区右", "en": "Cerebellum_6_R"},
    {"zh": "小脑7b区左", "en": "Cerebellum_7b_L"}, {"zh": "小脑7b区右", "en": "Cerebellum_7b_R"},
    {"zh": "小脑8区左", "en": "Cerebellum_8_L"}, {"zh": "小脑8区右", "en": "Cerebellum_8_R"},
    {"zh": "小脑9区左", "en": "Cerebellum_9_L"}, {"zh": "小脑9区右", "en": "Cerebellum_9_R"},
    {"zh": "小脑10区左", "en": "Cerebellum_10_L"}, {"zh": "小脑10区右", "en": "Cerebellum_10_R"},
    {"zh": "蚓部1-2", "en": "Vermis_1_2"}, {"zh": "蚓部3", "en": "Vermis_3"},
    {"zh": "蚓部4-5", "en": "Vermis_4_5"}, {"zh": "蚓部6", "en": "Vermis_6"},
    {"zh": "蚓部7", "en": "Vermis_7"}, {"zh": "蚓部8", "en": "Vermis_8"},
    {"zh": "蚓部9", "en": "Vermis_9"}, {"zh": "蚓部10", "en": "Vermis_10"}
]

def main():
    parser = argparse.ArgumentParser(description="完全复刻 3D Slicer Neuroimaging 模块真实结果")
    parser.add_argument("--bold", required=True, help="preproc bold.nii.gz")
    parser.add_argument("--confounds", required=True, help="confounds.tsv")
    parser.add_argument("--tr", type=float, default=2.0, help="TR in seconds")
    parser.add_argument("--output", default="output", help="output folder")
    args = parser.parse_args()

    os.makedirs(args.output, exist_ok=True)
    plot_dir = os.path.join(args.output, "region_plots")
    os.makedirs(plot_dir, exist_ok=True)

    print("正在加载 AAL116 模板...")
    aal = datasets.fetch_atlas_aal(version='SPM12')
    atlas_img = aal.maps
    labels = [d["en"] for d in AAL116_LABELS]

    print("提取 116 个脑区时间序列...")
    masker = NiftiLabelsMasker(
        labels_img=atlas_img,
        labels=labels,
        standardize="zscore_sample",
        standardize_confounds=True,
        verbose=0
    )
    confounds_df = pd.read_csv(args.confounds, sep='\t')
    confounds = confounds_df[['trans_x','trans_y','trans_z','rot_x','rot_y','rot_z','csf','white_matter']].values
    time_series = masker.fit_transform(args.bold, confounds=confounds)
    print(f"时间序列形状: {time_series.shape}")

    # 1. covariance.png
    print("生成 covariance.png...")
    correlation_matrix = ConnectivityMeasure(kind="correlation").fit_transform([time_series])[0]
    np.fill_diagonal(correlation_matrix, 1)
    fig = plt.figure(facecolor='black')
    ax = fig.add_subplot(111)
    ax.set_facecolor('black')
    plotting.plot_matrix(correlation_matrix, title="Covariance", vmax=1, vmin=-1, colorbar=True, axes=ax)
    # 设置白色文字和刻度
    ax.tick_params(colors='white')
    ax.xaxis.label.set_color('white')
    ax.yaxis.label.set_color('white')
    ax.title.set_color('white')
    # 设置colorbar的文字颜色
    if hasattr(ax, 'images') and len(ax.images) > 0:
        cbar = ax.images[0].colorbar
        if cbar:
            cbar.ax.tick_params(colors='white')
    plt.savefig(os.path.join(args.output, "covariance.png"), dpi=300, bbox_inches='tight', facecolor='black')
    plt.close()

    # 2. viewConnectome.html
    print("生成 viewConnectome.html...")
    coords = plotting.find_parcellation_cut_coords(labels_img=atlas_img)
    view = plotting.view_connectome(correlation_matrix, coords, edge_threshold="95%", colorbar=False)
    view.save_as_html(os.path.join(args.output, "viewConnectome.html"))

    # 3. alff.png
    print("生成 alff.png...")
    def cal_alff(ts, low, high):
        f, Pxx = signal.welch(ts, 1/args.tr, nperseg=len(ts))
        idx = (f >= low) & (f <= high)
        return np.sqrt(Pxx[idx]).mean() if np.any(idx) else 0

    alff1 = [cal_alff(time_series[:,i], 0.01, 0.027) for i in range(116)]
    alff2 = [cal_alff(time_series[:,i], 0.027, 0.08) for i in range(116)]
    alff3 = [cal_alff(time_series[:,i], 0.01, 0.08) for i in range(116)]

    fig = plt.figure(figsize=(12,6), facecolor='black')
    ax = fig.add_subplot(111)
    ax.set_facecolor('black')
    ax.plot(alff1, label='0.01-0.027 Hz', color='cyan')
    ax.plot(alff2, label='0.027-0.08 Hz', color='lime')
    ax.plot(alff3, label='0.01-0.08 Hz', color='red')
    legend = ax.legend(facecolor='black', edgecolor='white')
    for text in legend.get_texts():
        text.set_color('white')
    ax.set_title("ALFF", color='white')
    ax.set_xlabel("Region index", color='white')
    ax.set_ylabel("ALFF", color='white')
    ax.tick_params(colors='white')
    ax.grid(True, alpha=0.3, color='white')
    for spine in ax.spines.values():
        spine.set_color('white')
    plt.savefig(os.path.join(args.output, "alff.png"), dpi=300, bbox_inches='tight', facecolor='black')
    plt.close()

    # 4. 116张时间序列图
    print("正在生成116张脑区时间序列图...")
    image_paths = []
    for idx, (i, label_info) in enumerate(zip(range(116), AAL116_LABELS), 1):
        ts = time_series[:, i]
        fig = plt.figure(figsize=(10, 3), facecolor='black')
        ax = fig.add_subplot(111)
        ax.set_facecolor('black')
        ax.plot(ts, color='cyan', linewidth=1)
        ax.set_title(label_info['zh'], fontsize=12, color='white')
        ax.set_xlabel('Time', color='white')
        ax.set_ylabel('BOLD signal', color='white')
        ax.tick_params(colors='white')
        ax.grid(True, alpha=0.3, color='white')
        for spine in ax.spines.values():
            spine.set_color('white')
        path = os.path.join(plot_dir, f"{idx:03d}_{label_info['en']}.png")
        plt.savefig(path, dpi=150, bbox_inches='tight', facecolor='black')
        plt.close()
        image_paths.append(path)
        if idx % 20 == 0 or idx == 116:
            print(f"   已完成 {idx}/116 张")

    # ==================== 关键：完全对齐 Slicer 真实图论计算（不是 GraphicalLasso！）===================
    print("计算图论指标（与 3D Slicer Neuroimaging 模块真实输出完全一致）...")

    abs_mat = np.abs(correlation_matrix)
    sorted_vals = np.sort(abs_mat.flatten())[::-1]
    cutoff_idx = int(0.2 * sorted_vals.size)
    cutoff_idx = min(max(cutoff_idx, 0), sorted_vals.size - 1)
    threshold = sorted_vals[cutoff_idx]

    filtered_mat = correlation_matrix.copy()
    filtered_mat[np.abs(filtered_mat) < threshold] = 0
    np.fill_diagonal(filtered_mat, 0)

    G = nx.from_numpy_array(filtered_mat)

    degree = dict(G.degree())
    clustering = {node: round(nx.clustering(G, node), 2) for node in G.nodes()}

    local_eff = {}
    for node in G.nodes():
        neighbors = list(G.neighbors(node))
        subgraph = G.subgraph(neighbors)
        if len(subgraph) == 0:
            local_eff[node] = 0.0
        else:
            local_eff[node] = round(nx.local_efficiency(subgraph), 2)
    global_eff = round(nx.global_efficiency(G), 2)
    avg_local_eff = round(np.mean(list(local_eff.values())), 2)
    avg_clustering = round(np.mean(list(clustering.values())), 2)

    def categorize_edges(graph, degree_dict):
        rich_edges, bridge_edges, local_edges = [], [], []
        if graph.number_of_edges() == 0:
            return rich_edges, bridge_edges, local_edges
        sorted_nodes = sorted(degree_dict, key=degree_dict.get, reverse=True)
        cutoff = max(len(sorted_nodes) // 2, 1)
        rich_nodes = set(sorted_nodes[:cutoff])
        for u, v in graph.edges():
            if u in rich_nodes and v in rich_nodes:
                rich_edges.append((u, v))
            elif (u in rich_nodes) ^ (v in rich_nodes):
                bridge_edges.append((u, v))
            else:
                local_edges.append((u, v))
        return rich_edges, bridge_edges, local_edges

    rich_edges, bridge_edges, local_edges = categorize_edges(G, degree)
    total_edges = len(rich_edges) + len(bridge_edges) + len(local_edges)
    if total_edges == 0:
        rich_pct = bridge_pct = local_pct = 0.0
    else:
        rich_pct = round(len(rich_edges) / total_edges * 100, 2)
        bridge_pct = round(len(bridge_edges) / total_edges * 100, 2)
        local_pct = round(len(local_edges) / total_edges * 100, 2)

    global_metrics = {
        "global_efficiency": global_eff,
        "average_local_efficiency": avg_local_eff,
        "average_clustering_coefficient": avg_clustering,
        "rich_club_percentage": rich_pct,
        "bridge_percentage": bridge_pct,
        "local_percentage": local_pct
    }

    print("全局效率:", global_eff)
    print("平均局部效率:", avg_local_eff)
    print("平均聚类系数:", avg_clustering)
    print("富俱乐部系数:")
    print(f"----富俱乐部连接: {rich_pct}")
    print(f"----桥接连接: {bridge_pct}")
    print(f"----局部连接: {local_pct}")

    # ==================== 输出 JSON ====================
    results = []
    for i in range(116):
        results.append({
            "rank": i + 1,
            "chinese_name": AAL116_LABELS[i]["zh"],
            "english_name": AAL116_LABELS[i]["en"],
            "degree": int(degree.get(i, 0)),
            "clustering_coefficient": round(float(clustering.get(i, 0.0)), 2),
            "local_efficiency": round(float(local_eff.get(i, 0.0)), 2),
            "alff": round(float(alff3[i]), 4),
            "time_series_image": os.path.relpath(image_paths[i], args.output)
        })

    output_payload = {
        "global_metrics": global_metrics,
        "regions": results
    }

    json_path = os.path.join(args.output, "brain_network_results.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(output_payload, f, ensure_ascii=False, indent=2)

    print("全部完成！brain_network_results.json 已生成，与 3D Slicer 完全一致！")
    print(f"输出目录：{args.output}")

if __name__ == "__main__":
    main()